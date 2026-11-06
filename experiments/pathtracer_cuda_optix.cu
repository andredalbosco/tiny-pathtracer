// One source, compiled once for the host and once as OptiX device PTX.
#include "cuda_common.cuh"
#include <optix.h>

struct Parameters {
    const Scene* scene;
    float* pixels;
    OptixTraversableHandle geometry;
    int samples, width;
};

#ifdef TINY_OPTIX_DEVICE
extern "C" { __constant__ Parameters params; }

__device__ Path* payload_path() {
    uint64_t address = uint64_t(optixGetPayload_0())
                     | (uint64_t(optixGetPayload_1()) << 32);
    return reinterpret_cast<Path*>(address);
}
extern "C" __global__ void __miss__radiance() {
    optixSetPayload_2(0);
}
extern "C" __global__ void __closesthit__radiance() {
    Path& path = *payload_path();
    float2 uv = optixGetTriangleBarycentrics();
    path.hit = {optixGetRayTmax(), uv.x, uv.y, int(optixGetPrimitiveIndex())};
    optixSetPayload_2(shade(*params.scene, path) ? 1 : 0);
}
extern "C" __global__ void __raygen__render() {
    uint3 index = optixGetLaunchIndex();
    int pixel = int(index.y) * params.width + int(index.x);
    Float3 sum{};
    for (int s = 0; s < params.samples; ++s) {
        Path path;
        path.ray = camera_ray(params.scene->camera, pixel, s, params.width,
                              params.width, path.random);
        uint64_t address = reinterpret_cast<uint64_t>(&path);
        for (int depth = 0; depth < max_depth; ++depth) {
            unsigned low = unsigned(address), high = unsigned(address >> 32), again = 0;
            Ray ray = path.ray;
            optixTrace(params.geometry,
                       make_float3(ray.origin.x, ray.origin.y, ray.origin.z),
                       make_float3(ray.direction.x, ray.direction.y, ray.direction.z),
                       1e-4f, 1e30f, 0.0f, 255,
                       OPTIX_RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
                       0, 1, 0, low, high, again);
            if (!again) break;
        }
        sum += path.color;
    }
    write_pixel(params.pixels, pixel, sum * (1.0f / params.samples));
}
#else
#include <optix_function_table_definition.h>
#include <optix_stubs.h>
#include <optix_stack_size.h>
#include <fstream>
#include <iterator>

void optix_check(OptixResult result) {
    if (result != OPTIX_SUCCESS) {
        std::cerr << "OptiX: " << optixGetErrorName(result) << '\n';
        std::exit(1);
    }
}
void log_message(unsigned, const char* tag, const char* message, void*) {
    std::cerr << "OptiX [" << tag << "]: " << message << '\n';
}
struct alignas(OPTIX_SBT_RECORD_ALIGNMENT) Record {
    char header[OPTIX_SBT_RECORD_HEADER_SIZE];
};
int main(int argc, char** argv) {
    Settings s = settings(argc, argv, "build/optix.hdr");
    Scene scene = cornell_box();
    Buffers b(scene, s.width);
    optix_check(optixInit());
    OptixDeviceContextOptions context_options{};
    context_options.logCallbackFunction = log_message;
    context_options.logCallbackLevel = 2;
    OptixDeviceContext context;
    optix_check(optixDeviceContextCreate(nullptr, &context_options, &context));

    // Native triangles: OptiX builds the BVH and uses hardware intersections.
    std::vector<float3> vertices;
    for (int i = 0; i < scene.triangle_count; ++i) {
        const Triangle& t = scene.triangles[i];
        for (Float3 p : {t.p, t.p+t.e1, t.p+t.e2})
            vertices.push_back(make_float3(p.x, p.y, p.z));
    }
    float3* device_vertices;
    cuda_check(cudaMalloc(&device_vertices, vertices.size()*sizeof(float3)));
    cuda_check(cudaMemcpy(device_vertices, vertices.data(), vertices.size()*sizeof(float3),
                          cudaMemcpyHostToDevice));
    CUdeviceptr vertex_buffer = reinterpret_cast<CUdeviceptr>(device_vertices);
    unsigned geometry_flags = OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT;
    OptixBuildInput input{};
    input.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
    input.triangleArray.vertexBuffers = &vertex_buffer;
    input.triangleArray.numVertices = unsigned(vertices.size());
    input.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
    input.triangleArray.vertexStrideInBytes = sizeof(float3);
    input.triangleArray.flags = &geometry_flags;
    input.triangleArray.numSbtRecords = 1;
    OptixAccelBuildOptions build_options{};
    build_options.operation = OPTIX_BUILD_OPERATION_BUILD;
    OptixAccelBufferSizes sizes;
    optix_check(optixAccelComputeMemoryUsage(context, &build_options, &input, 1, &sizes));
    void* scratch; void* gas;
    cuda_check(cudaMalloc(&scratch, sizes.tempSizeInBytes));
    cuda_check(cudaMalloc(&gas, sizes.outputSizeInBytes));
    OptixTraversableHandle geometry;
    optix_check(optixAccelBuild(context, nullptr, &build_options, &input, 1,
        reinterpret_cast<CUdeviceptr>(scratch), sizes.tempSizeInBytes,
        reinterpret_cast<CUdeviceptr>(gas), sizes.outputSizeInBytes, &geometry, nullptr, 0));
    cuda_check(cudaDeviceSynchronize());
    cuda_check(cudaFree(scratch));

    std::string ptx_name = std::string(argv[0]) + ".ptx";
    std::ifstream file(ptx_name);
    if (!file) { std::cerr << "Cannot read " << ptx_name << '\n'; return 1; }
    std::string ptx((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    OptixModuleCompileOptions module_options{};
    OptixPipelineCompileOptions compile_options{};
    compile_options.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS;
    compile_options.numPayloadValues = 3;
    compile_options.numAttributeValues = 2;
    compile_options.pipelineLaunchParamsVariableName = "params";
    compile_options.usesPrimitiveTypeFlags = static_cast<unsigned int>(OPTIX_PRIMITIVE_TYPE_FLAGS_TRIANGLE);
    OptixModule module;
    optix_check(optixModuleCreate(context, &module_options, &compile_options,
                                  ptx.data(), ptx.size(), nullptr, nullptr, &module));
    OptixProgramGroupDesc descriptions[3]{};
    descriptions[0].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    descriptions[0].raygen.module = module;
    descriptions[0].raygen.entryFunctionName = "__raygen__render";
    descriptions[1].kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
    descriptions[1].miss.module = module;
    descriptions[1].miss.entryFunctionName = "__miss__radiance";
    descriptions[2].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    descriptions[2].hitgroup.moduleCH = module;
    descriptions[2].hitgroup.entryFunctionNameCH = "__closesthit__radiance";
    OptixProgramGroupOptions group_options{};
    OptixProgramGroup groups[3];
    optix_check(optixProgramGroupCreate(context, descriptions, 3, &group_options,
                                        nullptr, nullptr, groups));
    OptixPipelineLinkOptions link_options{};
    link_options.maxTraceDepth = 1; // Iterative bounces; no recursive optixTrace.
    OptixPipeline pipeline;
    optix_check(optixPipelineCreate(context, &compile_options, &link_options,
                                    groups, 3, nullptr, nullptr, &pipeline));
    OptixStackSizes stack{};
    for (auto group : groups) optix_check(optixUtilAccumulateStackSizes(group, &stack, pipeline));
    unsigned traversal, state, continuation;
    optix_check(optixUtilComputeStackSizes(&stack, 1, 0, 0, &traversal, &state, &continuation));
    optix_check(optixPipelineSetStackSize(pipeline, traversal, state, continuation, 1));

    Record records[3]{};
    for (int i = 0; i < 3; ++i) optix_check(optixSbtRecordPackHeader(groups[i], &records[i]));
    Record* device_records;
    cuda_check(cudaMalloc(&device_records, sizeof(records)));
    cuda_check(cudaMemcpy(device_records, records, sizeof(records), cudaMemcpyHostToDevice));
    OptixShaderBindingTable sbt{};
    sbt.raygenRecord = reinterpret_cast<CUdeviceptr>(device_records);
    sbt.missRecordBase = reinterpret_cast<CUdeviceptr>(device_records + 1);
    sbt.missRecordStrideInBytes = sizeof(Record); sbt.missRecordCount = 1;
    sbt.hitgroupRecordBase = reinterpret_cast<CUdeviceptr>(device_records + 2);
    sbt.hitgroupRecordStrideInBytes = sizeof(Record); sbt.hitgroupRecordCount = 1;
    Parameters parameters{b.scene, b.pixels, geometry, s.samples, s.width};
    Parameters* device_parameters;
    cuda_check(cudaMalloc(&device_parameters, sizeof(Parameters)));
    cuda_check(cudaMemcpy(device_parameters, &parameters, sizeof(Parameters), cudaMemcpyHostToDevice));
    optix_check(optixLaunch(pipeline, nullptr, reinterpret_cast<CUdeviceptr>(device_parameters),
                            sizeof(Parameters), &sbt, s.width, s.width, 1));
    finish(s, b);
    cuda_check(cudaFree(device_parameters)); cuda_check(cudaFree(device_records));
    optix_check(optixPipelineDestroy(pipeline));
    for (auto group : groups) optix_check(optixProgramGroupDestroy(group));
    optix_check(optixModuleDestroy(module));
    cuda_check(cudaFree(gas)); cuda_check(cudaFree(device_vertices));
    optix_check(optixDeviceContextDestroy(context));
}
#endif
