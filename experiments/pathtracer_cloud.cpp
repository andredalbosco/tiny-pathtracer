#include "pathtracer_cloud.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include "../external/cuda/helper_math.h"
#include <optix_function_table_definition.h>
#include <optix_stubs.h>
#include <nanovdb/util/IO.h>
#include <algorithm>
#include <chrono>
#include <vector>
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

void saveImage(const std::string& filename, const std::vector<float4>& pixels, int width) {
    std::vector<float> rgb(pixels.size() * 3);
    for (size_t i = 0; i < pixels.size(); ++i) {
        rgb[3*i] = pixels[i].x;
        rgb[3*i+1] = pixels[i].y;
        rgb[3*i+2] = pixels[i].z;
    }

    if (filename.size() >= 4 && filename.substr(filename.size()-4) == ".f32") {
        std::ofstream image(filename, std::ios::binary);
        image.write(reinterpret_cast<const char*>(rgb.data()), rgb.size() * sizeof(float));
        image.close();
        if (!image) throw std::runtime_error("Cannot write " + filename);
    } else if (!stbi_write_hdr(filename.c_str(), width, width, 3, rgb.data())) {
        throw std::runtime_error("Cannot write " + filename);
    }
    std::cout << "Saved " << filename << '\n';
}

Volume makeVolume(OptixDeviceContext context, float3 halfBounds) {
    Volume volume {};
    OptixAabb aabb {
        -halfBounds.x, -halfBounds.y, -halfBounds.z,
         halfBounds.x,  halfBounds.y,  halfBounds.z
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
    compileOptions.numPayloadValues = 2;
    compileOptions.numAttributeValues = 1;
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
    descriptions[1].miss.entryFunctionName = "__miss__volume";

    descriptions[2].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    descriptions[2].hitgroup.moduleIS = pipeline.module;
    descriptions[2].hitgroup.entryFunctionNameIS = "__intersection__volume";
    descriptions[2].hitgroup.moduleCH = pipeline.module;
    descriptions[2].hitgroup.entryFunctionNameCH = "__closesthit__volume";

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

void render(const Pipeline& pipeline, const ShaderBindingTable& table,
            CloudParameters& parameters, int width) {
    size_t count = size_t(width) * width;
    check(cudaMalloc(&parameters.pixels, count * sizeof(float4)));

    CloudParameters* deviceParameters;
    check(cudaMalloc(&deviceParameters, sizeof(parameters)));
    check(cudaMemcpy(deviceParameters, &parameters, sizeof(parameters), cudaMemcpyHostToDevice));

    auto start = std::chrono::steady_clock::now();
    check(optixLaunch(pipeline.handle, nullptr, reinterpret_cast<CUdeviceptr>(deviceParameters),
                      sizeof(CloudParameters), &table.sbt, width, width, 1));
    check(cudaDeviceSynchronize());
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout << "Rendered " << width << "x" << width << " in " << seconds << " s\n";
    check(cudaFree(deviceParameters));
}

int positiveInt(const char* text) {
    size_t end;
    int value = std::stoi(text, &end);
    if (text[end] != '\0' || value < 1 || value > 16384)
        throw std::runtime_error("Expected an integer in 1..16384");
    return value;
}

int main(int argc, char** argv) try {
    if (argc > 7 || (argc == 2 && std::string(argv[1]) == "--help")) {
        std::cerr << "Usage: " << argv[0]
                  << " [cloud.nvdb] [output.hdr|output.f32] [width] [steps] [shadow_steps] [extinction]\n";
        return argc > 7 ? 1 : 0;
    }

    const char* filename = argc > 1 ? argv[1] : "../volume-assets/wdas-cloud/wdas_cloud_quarter.nvdb";
    const char* output = argc > 2 ? argv[2] : "build/cloud.hdr";
    int width = argc > 3 ? positiveInt(argv[3]) : 640;
    CloudParameters parameters {};
    parameters.steps = argc > 4 ? positiveInt(argv[4]) : 256;
    parameters.shadowSteps = argc > 5 ? positiveInt(argv[5]) : 64;
    parameters.extinction = 4;
    if (argc > 6) {
        size_t end;
        parameters.extinction = std::stof(argv[6], &end);
        if (argv[6][end] != '\0' || !std::isfinite(parameters.extinction) || parameters.extinction < 0)
            throw std::runtime_error("Expected finite, nonnegative extinction");
    }

    auto handle = nanovdb::io::readGrid(filename, std::string("density"));
    const auto* grid = handle.grid<float>();
    if (!grid || !grid->isFogVolume() || grid->activeVoxelCount() == 0 || grid->tree().background() != 0)
        throw std::runtime_error("Expected a nonempty, zero-background float fog grid named density");

    auto bounds = grid->worldBBox();
    auto center = (bounds.min() + bounds.max()) * 0.5;
    auto extent = bounds.max() - bounds.min();
    parameters.center = make_float3(center[0], center[1], center[2]);
    parameters.scale = float(std::max({extent[0], extent[1], extent[2]})) * 0.5f;
    if (!std::isfinite(parameters.scale) || parameters.scale <= 0)
        throw std::runtime_error("Expected finite, nonempty grid bounds");
    parameters.halfBounds = make_float3(extent[0], extent[1], extent[2]) / (2 * parameters.scale);

    std::cout << "Loaded " << filename << " (" << handle.size() << " bytes)\n";
    std::string ptx = readFile(std::string(argv[0]) + ".ptx");

    check(cudaFree(nullptr));
    check(optixInit());
    OptixDeviceContext context;
    check(optixDeviceContextCreate(nullptr, nullptr, &context));

    nanovdb::FloatGrid* deviceGrid;
    check(cudaMalloc(&deviceGrid, handle.size()));
    check(cudaMemcpy(deviceGrid, handle.data(), handle.size(), cudaMemcpyHostToDevice));
    parameters.grid = deviceGrid;

    Volume volume = makeVolume(context, parameters.halfBounds);
    parameters.volume = volume.handle;
    Pipeline pipeline = makePipeline(context, ptx);
    ShaderBindingTable table = makeShaderBindingTable(pipeline);

    render(pipeline, table, parameters, width);
    std::vector<float4> pixels(size_t(width) * width);
    check(cudaMemcpy(pixels.data(), parameters.pixels, pixels.size() * sizeof(float4), cudaMemcpyDeviceToHost));

    check(cudaFree(parameters.pixels));
    check(cudaFree(table.records));
    check(optixPipelineDestroy(pipeline.handle));
    for (auto group : pipeline.groups) check(optixProgramGroupDestroy(group));
    check(optixModuleDestroy(pipeline.module));
    check(cudaFree(volume.accelerationBuffer));
    check(cudaFree(deviceGrid));
    check(optixDeviceContextDestroy(context));

    saveImage(output, pixels, width);
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
