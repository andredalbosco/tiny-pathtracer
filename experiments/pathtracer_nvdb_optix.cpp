#include "pathtracer_nvdb_optix.h"
#include "../external/cuda/helper_math.h"
#include <optix_function_table_definition.h>
#include <optix_stubs.h>
#include <nanovdb/util/IO.h>
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

struct alignas(OPTIX_SBT_RECORD_ALIGNMENT) Record {
    char header[OPTIX_SBT_RECORD_HEADER_SIZE];
};

struct Volume {
    void* accelerationBuffer;
    OptixTraversableHandle handle;
};

struct Pipeline {
    OptixModule module;
    OptixProgramGroup groups[3];
    OptixPipeline handle;
};

struct ShaderBindingTable {
    Record* records;
    OptixShaderBindingTable sbt;
};

void check(cudaError_t error) {
    if (error != cudaSuccess) {
        std::cerr << "CUDA: " << cudaGetErrorString(error) << '\n';
        std::exit(1);
    }
}

void check(OptixResult error) {
    if (error != OPTIX_SUCCESS) {
        std::cerr << "OptiX: " << optixGetErrorName(error) << '\n';
        std::exit(1);
    }
}

std::string readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) { std::cerr << "Cannot read " << filename << '\n'; std::exit(1); }

    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void saveImage(const char* filename, const uchar3* pixels, int width, int height) {
    std::ofstream image(filename, std::ios::binary);
    image << "P6\n" << width << ' ' << height << "\n255\n";
    image.write(reinterpret_cast<const char*>(pixels), width * height * 3);
    image.close();

    if (!image) { std::cerr << "Cannot write " << filename << '\n'; std::exit(1); }

    std::cout << "Saved " << filename << '\n';
}

Volume makeVolume(OptixDeviceContext context, const nanovdb::FloatGrid& grid) {
    Volume volume {};
    auto bounds = grid.worldBBox();
    OptixAabb aabb {
        float(bounds.min()[0]), float(bounds.min()[1]), float(bounds.min()[2]),
        float(bounds.max()[0]), float(bounds.max()[1]), float(bounds.max()[2])
    };

    OptixAabb* deviceBounds;
    check(cudaMalloc(&deviceBounds, sizeof(aabb)));
    check(cudaMemcpy(deviceBounds, &aabb, sizeof(aabb), cudaMemcpyHostToDevice));

    CUdeviceptr boundsBuffer = reinterpret_cast<CUdeviceptr>(deviceBounds);
    unsigned flags = OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT;
    OptixBuildInput input {};
    input.type = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
    input.customPrimitiveArray.aabbBuffers = &boundsBuffer;
    input.customPrimitiveArray.numPrimitives = 1;
    input.customPrimitiveArray.flags = &flags;
    input.customPrimitiveArray.numSbtRecords = 1;

    OptixAccelBuildOptions buildOptions {};
    buildOptions.operation = OPTIX_BUILD_OPERATION_BUILD;

    OptixAccelBufferSizes sizes {};
    check(optixAccelComputeMemoryUsage(context, &buildOptions, &input, 1, &sizes));

    void* scratch;
    check(cudaMalloc(&scratch, sizes.tempSizeInBytes));
    check(cudaMalloc(&volume.accelerationBuffer, sizes.outputSizeInBytes));

    check(optixAccelBuild(context, nullptr, &buildOptions, &input, 1,
        reinterpret_cast<CUdeviceptr>(scratch), sizes.tempSizeInBytes,
        reinterpret_cast<CUdeviceptr>(volume.accelerationBuffer), sizes.outputSizeInBytes,
        &volume.handle, nullptr, 0));
    check(cudaDeviceSynchronize());
    check(cudaFree(scratch));
    check(cudaFree(deviceBounds));

    return volume;
}

Pipeline makePipeline(OptixDeviceContext context, const std::string& ptx) {
    Pipeline pipeline {};

    OptixModuleCompileOptions moduleOptions {};
    OptixPipelineCompileOptions compileOptions {};
    compileOptions.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS;
    compileOptions.numPayloadValues = 1;
    compileOptions.numAttributeValues = 0;
    compileOptions.pipelineLaunchParamsVariableName = "params";
    compileOptions.usesPrimitiveTypeFlags = unsigned(OPTIX_PRIMITIVE_TYPE_FLAGS_CUSTOM);
    check(optixModuleCreate(context, &moduleOptions, &compileOptions,
                            ptx.data(), ptx.size(), nullptr, nullptr, &pipeline.module));

    OptixProgramGroupDesc descriptions[3] {};

    descriptions[0].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    descriptions[0].raygen.module = pipeline.module;
    descriptions[0].raygen.entryFunctionName = "__raygen__render";

    descriptions[1].kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
    descriptions[1].miss.module = pipeline.module;
    descriptions[1].miss.entryFunctionName = "__miss__cloud";

    descriptions[2].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    descriptions[2].hitgroup.moduleIS = pipeline.module;
    descriptions[2].hitgroup.entryFunctionNameIS = "__intersection__cloud";
    descriptions[2].hitgroup.moduleCH = pipeline.module;
    descriptions[2].hitgroup.entryFunctionNameCH = "__closesthit__cloud";

    OptixProgramGroupOptions groupOptions {};
    check(optixProgramGroupCreate(context, descriptions, 3, &groupOptions,
                                  nullptr, nullptr, pipeline.groups));

    OptixPipelineLinkOptions linkOptions {};
    linkOptions.maxTraceDepth = 1;
    check(optixPipelineCreate(context, &compileOptions, &linkOptions,
                              pipeline.groups, 3, nullptr, nullptr, &pipeline.handle));

    return pipeline;
}

ShaderBindingTable makeShaderBindingTable(const Pipeline& pipeline) {
    ShaderBindingTable table {};

    Record records[3] {};
    for (int i = 0; i < 3; ++i) check(optixSbtRecordPackHeader(pipeline.groups[i], &records[i]));

    check(cudaMalloc(&table.records, sizeof(records)));
    check(cudaMemcpy(table.records, records, sizeof(records), cudaMemcpyHostToDevice));

    table.sbt.raygenRecord = reinterpret_cast<CUdeviceptr>(table.records);
    table.sbt.missRecordBase = reinterpret_cast<CUdeviceptr>(table.records + 1);
    table.sbt.missRecordStrideInBytes = sizeof(Record);
    table.sbt.missRecordCount = 1;
    table.sbt.hitgroupRecordBase = reinterpret_cast<CUdeviceptr>(table.records + 2);
    table.sbt.hitgroupRecordStrideInBytes = sizeof(Record);
    table.sbt.hitgroupRecordCount = 1;

    return table;
}

Camera makeCamera(float3 position, float3 target, float3 up, float verticalFov) {
    Camera camera {};
    camera.position = position;
    camera.forward = normalize(target - position);
    camera.right = normalize(cross(up, camera.forward));
    camera.up = cross(camera.forward, camera.right);
    camera.tanHalfFov = std::tan(verticalFov * M_PI / 360);

    return camera;
}

uchar3* render(const Pipeline& pipeline, const ShaderBindingTable& table,
               const Volume& volume, const nanovdb::FloatGrid* grid, const Camera& camera,
               int width, int height) {
    Parameters parameters {};
    parameters.scene = volume.handle;
    parameters.grid = grid;
    parameters.camera = camera;
    check(cudaMallocManaged(&parameters.pixels, width * height * sizeof(uchar3)));

    Parameters* deviceParameters;
    check(cudaMalloc(&deviceParameters, sizeof(parameters)));
    check(cudaMemcpy(deviceParameters, &parameters, sizeof(parameters), cudaMemcpyHostToDevice));

    check(optixLaunch(pipeline.handle, nullptr, reinterpret_cast<CUdeviceptr>(deviceParameters),
                      sizeof(Parameters), &table.sbt, width, height, 1));
    check(cudaDeviceSynchronize());

    check(cudaFree(deviceParameters));

    return parameters.pixels;
}

int main(int argc, char** argv) try {
    if (argc > 3) {
        std::cerr << "Usage: " << argv[0] << " [cloud.nvdb] [output.ppm]\n";
        return 1;
    }

    const int width = 640, height = 640;
    const char* filename = argc > 1 ? argv[1] : "../volume-assets/wdas-cloud/wdas_cloud_quarter.nvdb";
    const char* output = argc > 2 ? argv[2] : "build/nvdb-cloud.ppm";
    auto handle = nanovdb::io::readGrid(filename, std::string("density"));
    const auto* grid = handle.grid<float>();
    if (!grid || !grid->isFogVolume() || grid->activeVoxelCount() == 0 || grid->tree().background() != 0)
        throw std::runtime_error("Expected a nonempty, zero-background float fog grid named density");

    std::cout << "Loaded " << filename << " (" << handle.size() << " bytes)\n";
    std::string ptx = readFile(std::string(argv[0]) + ".ptx");

    check(cudaFree(nullptr));
    check(optixInit());
    OptixDeviceContext context;
    check(optixDeviceContextCreate(nullptr, nullptr, &context));

    nanovdb::FloatGrid* deviceGrid;
    check(cudaMalloc(&deviceGrid, handle.size()));
    check(cudaMemcpy(deviceGrid, handle.data(), handle.size(), cudaMemcpyHostToDevice));

    Volume volume = makeVolume(context, *grid);
    Pipeline pipeline = makePipeline(context, ptx);
    ShaderBindingTable table = makeShaderBindingTable(pipeline);

    auto bounds = grid->worldBBox();
    auto center = (bounds.min() + bounds.max()) * 0.5;
    auto extent = bounds.max() - bounds.min();
    float scale = float(std::max({extent[0], extent[1], extent[2]})) * 0.5f;
    float3 target = make_float3(center[0], center[1], center[2]);
    Camera camera = makeCamera(target + make_float3(3.2f, 0.32f, 0) * scale,
                               target, {0, 1, 0}, 40);

    uchar3* pixels = render(pipeline, table, volume, deviceGrid, camera, width, height);
    saveImage(output, pixels, width, height);

    check(cudaFree(pixels));
    check(cudaFree(table.records));
    check(optixPipelineDestroy(pipeline.handle));
    for (auto group : pipeline.groups) check(optixProgramGroupDestroy(group));
    check(optixModuleDestroy(pipeline.module));
    check(cudaFree(volume.accelerationBuffer));
    check(cudaFree(deviceGrid));
    check(optixDeviceContextDestroy(context));
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
