#include "pathtracer_optix.h"
#include <optix_function_table_definition.h>
#include <optix_stubs.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

struct alignas(OPTIX_SBT_RECORD_ALIGNMENT) SbtRecord {
    char header[OPTIX_SBT_RECORD_HEADER_SIZE];
};

struct TriangleMesh {
    float3* vertices;
    Surface* surfaces;
    void* acceleration_buffer;
    OptixTraversableHandle handle;
};

struct Pipeline {
    OptixModule module;
    OptixProgramGroup groups[3];
    OptixPipeline handle;
};

struct ShaderBindingTable {
    SbtRecord* records;
    OptixShaderBindingTable sbt;
};

float3 make_float3(Float3 value) {
    return {value.x, value.y, value.z};
}

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

TriangleMesh create_triangle_mesh(OptixDeviceContext context,
                                  const std::vector<Triangle>& triangles) {
    CHECK(!triangles.empty());
    TriangleMesh geometry {};
    std::vector<float3> vertices(triangles.size() * 3);
    std::vector<Surface> surfaces(triangles.size());

    for (std::size_t i = 0; i < triangles.size(); ++i) {
        const Triangle& triangle = triangles[i];
        vertices[3 * i] = make_float3(triangle.p);
        vertices[3 * i + 1] = make_float3(triangle.p + triangle.e1);
        vertices[3 * i + 2] = make_float3(triangle.p + triangle.e2);
        surfaces[i] = {
            triangle.normal,
            triangle.albedo,
            triangle.emission
        };
    }

    std::size_t vertex_bytes = vertices.size() * sizeof(float3);
    std::size_t surface_bytes = surfaces.size() * sizeof(Surface);
    check(cudaMalloc(&geometry.vertices, vertex_bytes));
    check(cudaMemcpy(geometry.vertices, vertices.data(), vertex_bytes, cudaMemcpyHostToDevice));
    check(cudaMalloc(&geometry.surfaces, surface_bytes));
    check(cudaMemcpy(geometry.surfaces, surfaces.data(), surface_bytes, cudaMemcpyHostToDevice));

    CUdeviceptr vertex_buffer = reinterpret_cast<CUdeviceptr>(geometry.vertices);
    unsigned flags = OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT;
    OptixBuildInput input {};
    input.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
    input.triangleArray.vertexBuffers = &vertex_buffer;
    input.triangleArray.numVertices = unsigned(vertices.size());
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
    check(cudaMalloc(&geometry.acceleration_buffer, sizes.outputSizeInBytes));

    check(optixAccelBuild(context, nullptr, &build_options, &input, 1,
        reinterpret_cast<CUdeviceptr>(scratch), sizes.tempSizeInBytes,
        reinterpret_cast<CUdeviceptr>(geometry.acceleration_buffer), sizes.outputSizeInBytes,
        &geometry.handle, nullptr, 0));
    check(cudaDeviceSynchronize());
    check(cudaFree(scratch));

    return geometry;
}

Pipeline create_pipeline(OptixDeviceContext context, const std::string& ptx) {
    Pipeline pipeline {};

    OptixModuleCompileOptions module_options {};
    OptixPipelineCompileOptions compile_options {};
    compile_options.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS;
    compile_options.numPayloadValues = 2;
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

ShaderBindingTable create_shader_binding_table(const Pipeline& pipeline) {
    ShaderBindingTable table {};

    SbtRecord records[3] {};
    for (int i = 0; i < 3; ++i) check(optixSbtRecordPackHeader(pipeline.groups[i], &records[i]));

    check(cudaMalloc(&table.records, sizeof(records)));
    check(cudaMemcpy(table.records, records, sizeof(records), cudaMemcpyHostToDevice));

    table.sbt.raygenRecord = reinterpret_cast<CUdeviceptr>(table.records);
    table.sbt.missRecordBase = reinterpret_cast<CUdeviceptr>(table.records + 1);
    table.sbt.missRecordStrideInBytes = sizeof(SbtRecord);
    table.sbt.missRecordCount = 1;
    table.sbt.hitgroupRecordBase = reinterpret_cast<CUdeviceptr>(table.records + 2);
    table.sbt.hitgroupRecordStrideInBytes = sizeof(SbtRecord);
    table.sbt.hitgroupRecordCount = 1;

    return table;
}

uchar3* render(const Pipeline& pipeline, const ShaderBindingTable& table,
               const TriangleMesh& geometry, const Camera& camera, const PointLight& light,
               int width, int height) {
    LaunchParams parameters {};
    parameters.scene = geometry.handle;
    parameters.surfaces = geometry.surfaces;
    parameters.camera = camera;
    parameters.light = light;
    check(cudaMallocManaged(&parameters.pixels, width * height * sizeof(uchar3)));

    LaunchParams* device_parameters;
    check(cudaMalloc(&device_parameters, sizeof(parameters)));
    check(cudaMemcpy(device_parameters, &parameters, sizeof(parameters), cudaMemcpyHostToDevice));

    check(optixLaunch(pipeline.handle, nullptr, reinterpret_cast<CUdeviceptr>(device_parameters),
                      sizeof(LaunchParams), &table.sbt, width, height, 1));
    check(cudaDeviceSynchronize());

    check(cudaFree(device_parameters));

    return parameters.pixels;
}

int main(int argc, char** argv) {
    const int width = 640, height = 640;
    const char* output = argc == 2 ? argv[1] : "build/optix-cornell.ppm";
    std::string ptx_name = std::string(argv[0]) + ".ptx";

    std::string ptx = read_file(ptx_name);

    check(cudaFree(nullptr));
    check(optixInit());
    OptixDeviceContext context;
    check(optixDeviceContextCreate(nullptr, nullptr, &context));
    std::cout << "Hello, OptiX!\n";

    std::vector<Triangle> triangles = make_cornell_box();
    TriangleMesh cornell_box_mesh = create_triangle_mesh(context, triangles);
    Pipeline pipeline = create_pipeline(context, ptx);

    ShaderBindingTable table = create_shader_binding_table(pipeline);

    Camera camera = make_camera({278, 278, -800}, {278, 278, 278}, {0, 1, 0}, 40, float(width) / height);
    PointLight light {{278, 553.5f, 279.5f}, {100000, 100000, 100000}};

    uchar3* pixels = render(pipeline, table, cornell_box_mesh, camera, light, width, height);
    write_ppm(output, reinterpret_cast<const unsigned char*>(pixels), width, height);

    check(cudaFree(pixels));
    check(cudaFree(table.records));

    check(optixPipelineDestroy(pipeline.handle));
    for (auto group : pipeline.groups) {
        check(optixProgramGroupDestroy(group));
    }
    check(optixModuleDestroy(pipeline.module));

    check(cudaFree(cornell_box_mesh.acceleration_buffer));
    check(cudaFree(cornell_box_mesh.vertices));
    check(cudaFree(cornell_box_mesh.surfaces));

    check(optixDeviceContextDestroy(context));
}
