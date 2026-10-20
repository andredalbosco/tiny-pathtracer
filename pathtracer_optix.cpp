#include "pathtracer_optix.h"
#include <optix_function_table_definition.h>
#include <optix_stubs.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

struct alignas(OPTIX_SBT_RECORD_ALIGNMENT) Record {
    char header[OPTIX_SBT_RECORD_HEADER_SIZE];
};

struct TriangleMesh {
    float3* vertices;
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

void save_image(const char* filename, const uchar3* pixels, int width, int height) {
    std::ofstream image(filename, std::ios::binary);
    image << "P6\n" << width << ' ' << height << "\n255\n";
    image.write(reinterpret_cast<const char*>(pixels), width * height * 3);
    image.close();

    if (!image) { std::cerr << "Cannot write " << filename << '\n'; std::exit(1); }

    std::cout << "Saved " << filename << '\n';
}

TriangleMesh make_triangle(OptixDeviceContext context) {
    TriangleMesh geometry {};

    float3 vertices[] = {
        {-0.8f, -0.7f, 0},
        { 0.8f, -0.7f, 0},
        { 0,     0.8f, 0}
    };
    check(cudaMalloc(&geometry.vertices, sizeof(vertices)));
    check(cudaMemcpy(geometry.vertices, vertices, sizeof(vertices), cudaMemcpyHostToDevice));

    CUdeviceptr vertex_buffer = reinterpret_cast<CUdeviceptr>(geometry.vertices);
    unsigned flags = OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT;
    OptixBuildInput input {};
    input.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
    input.triangleArray.vertexBuffers = &vertex_buffer;
    input.triangleArray.numVertices = 3;
    input.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
    input.triangleArray.vertexStrideInBytes = sizeof(float3);
    input.triangleArray.flags = &flags;
    input.triangleArray.numSbtRecords = 1;

    OptixAccelBuildOptions build_options {};
    build_options.operation = OPTIX_BUILD_OPERATION_BUILD;

    OptixAccelBufferSizes sizes;
    check(optixAccelComputeMemoryUsage(context, &build_options, &input, 1, &sizes));

    void* scratch;
    check(cudaMalloc(&scratch, sizes.tempSizeInBytes));
    check(cudaMalloc(&geometry.accelerationBuffer, sizes.outputSizeInBytes));

    check(optixAccelBuild(context, nullptr, &build_options, &input, 1,
        reinterpret_cast<CUdeviceptr>(scratch), sizes.tempSizeInBytes,
        reinterpret_cast<CUdeviceptr>(geometry.accelerationBuffer), sizes.outputSizeInBytes,
        &geometry.handle, nullptr, 0));
    check(cudaDeviceSynchronize());
    check(cudaFree(scratch));

    return geometry;
}

Pipeline make_pipeline(OptixDeviceContext context, const std::string& ptx) {
    Pipeline pipeline {};

    OptixModuleCompileOptions module_options {};
    OptixPipelineCompileOptions compile_options {};
    compile_options.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS;
    compile_options.numPayloadValues = 1;
    compile_options.numAttributeValues = 2;
    compile_options.pipelineLaunchParamsVariableName = "params";
    compile_options.usesPrimitiveTypeFlags = unsigned(OPTIX_PRIMITIVE_TYPE_FLAGS_TRIANGLE);
    check(optixModuleCreate(context, &module_options, &compile_options,
                            ptx.data(), ptx.size(), nullptr, nullptr, &pipeline.module));

    OptixProgramGroupDesc descriptions[3] {};

    descriptions[0].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    descriptions[0].raygen.module = pipeline.module;
    descriptions[0].raygen.entryFunctionName = "__raygen__render";

    descriptions[1].kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
    descriptions[1].miss.module = pipeline.module;
    descriptions[1].miss.entryFunctionName = "__miss__color";

    descriptions[2].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    descriptions[2].hitgroup.moduleCH = pipeline.module;
    descriptions[2].hitgroup.entryFunctionNameCH = "__closesthit__color";

    OptixProgramGroupOptions group_options {};
    check(optixProgramGroupCreate(context, descriptions, 3, &group_options,
                                  nullptr, nullptr, pipeline.groups));

    OptixPipelineLinkOptions link_options {};
    link_options.maxTraceDepth = 1;
    check(optixPipelineCreate(context, &compile_options, &link_options,
                              pipeline.groups, 3, nullptr, nullptr, &pipeline.handle));

    return pipeline;
}

ShaderBindingTable make_shader_binding_table(const Pipeline& pipeline) {
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

int main(int argc, char** argv) {
    const int width = 640, height = 480;
    const char* output = argc == 2 ? argv[1] : "build/optix-triangle.ppm";
    std::string ptxName = std::string(argv[0]) + ".ptx";
    std::ifstream file(ptxName);
    if (!file) { std::cerr << "Cannot read " << ptxName << '\n'; return 1; }
    std::string ptx((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    check(cudaFree(nullptr));
    check(optixInit());
    OptixDeviceContext context;
    check(optixDeviceContextCreate(nullptr, nullptr, &context));
    std::cout << "Hello, OptiX!\n";

    TriangleMesh geometry = make_triangle(context);
    Pipeline pipeline = make_pipeline(context, ptx);

    ShaderBindingTable table = make_shader_binding_table(pipeline);

    Parameters parameters {};
    parameters.triangle = geometry.handle;
    check(cudaMallocManaged(&parameters.pixels, width * height * sizeof(uchar3)));

    Parameters* deviceParameters;
    check(cudaMalloc(&deviceParameters, sizeof(parameters)));
    check(cudaMemcpy(deviceParameters, &parameters, sizeof(parameters), cudaMemcpyHostToDevice));

    check(optixLaunch(pipeline.handle, nullptr, reinterpret_cast<CUdeviceptr>(deviceParameters),
                      sizeof(Parameters), &table.sbt, width, height, 1));
    check(cudaDeviceSynchronize());
    save_image(output, parameters.pixels, width, height);

    check(cudaFree(deviceParameters));
    check(cudaFree(parameters.pixels));
    check(cudaFree(table.records));

    check(optixPipelineDestroy(pipeline.handle));
    for (auto group : pipeline.groups) check(optixProgramGroupDestroy(group));
    check(optixModuleDestroy(pipeline.module));

    check(cudaFree(geometry.accelerationBuffer));
    check(cudaFree(geometry.vertices));

    check(optixDeviceContextDestroy(context));
}
