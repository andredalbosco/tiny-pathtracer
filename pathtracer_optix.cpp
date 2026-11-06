#include "pathtracer_optix.h"
#include "external/cuda/helper_math.h"
#include <optix_function_table_definition.h>
#include <optix_stubs.h>
#include <cmath>
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
    Surface* surfaces;
    void* accelerationBuffer;
    OptixTraversableHandle handle;
};

struct Quad {
    float3 origin, u, v;
    uchar3 color;
    float3 emission {};
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

void setBlock(Quad* quads, float3 center, float3 size, float degrees, uchar3 color) {
    float angle = degrees * M_PI / 180;
    float c = std::cos(angle), s = std::sin(angle);
    float3 u {size.x * c, 0, -size.x * s};
    float3 v {size.z * s, 0, size.z * c};
    float3 up {0, size.y, 0};
    float3 p {center.x - (u.x + v.x) * 0.5f,
              center.y - size.y * 0.5f,
              center.z - (u.z + v.z) * 0.5f};

    quads[0] = {p, up, u, color};
    quads[1] = {{p.x + v.x, p.y, p.z + v.z}, u, up, color};
    quads[2] = {p, v, up, color};
    quads[3] = {{p.x + u.x, p.y, p.z + u.z}, up, v, color};
    quads[4] = {{p.x, p.y + size.y, p.z}, v, u, color};
    quads[5] = {p, u, v, color};
}

TriangleMesh makeCornellBox(OptixDeviceContext context) {
    TriangleMesh geometry {};

    uchar3 white {190, 190, 190};
    uchar3 red {180, 35, 35};
    uchar3 green {35, 150, 45};
    uchar3 block {220, 220, 220};

    Quad quads[18] {
        {{0, 0, 0}, {0, 555, 0}, {0, 0, 555}, red},
        {{555, 0, 0}, {0, 0, 555}, {0, 555, 0}, green},
        {{0, 0, 0}, {0, 0, 555}, {555, 0, 0}, white},
        {{0, 555, 0}, {555, 0, 0}, {0, 0, 555}, white},
        {{0, 0, 555}, {0, 555, 0}, {555, 0, 0}, white},
        {{213, 554, 227}, {130, 0, 0}, {0, 0, 105}, {255, 255, 255}, {1, 1, 1}}
    };

    setBlock(quads + 6, {370, 82.5f, 169}, {165, 165, 165}, 18, block);
    setBlock(quads + 12, {187, 165, 351}, {165, 330, 165}, -15, block);

    const int quadCount = sizeof(quads) / sizeof(Quad);
    float3 vertices[quadCount * 6];
    Surface surfaces[quadCount * 2];

    for (int i = 0; i < quadCount; ++i) {
        const Quad& q = quads[i];
        float3 a = q.origin;
        float3 b {a.x + q.u.x, a.y + q.u.y, a.z + q.u.z};
        float3 c {b.x + q.v.x, b.y + q.v.y, b.z + q.v.z};
        float3 d {a.x + q.v.x, a.y + q.v.y, a.z + q.v.z};

        vertices[6*i] = a;
        vertices[6*i + 1] = b;
        vertices[6*i + 2] = c;
        vertices[6*i + 3] = a;
        vertices[6*i + 4] = c;
        vertices[6*i + 5] = d;
        surfaces[2*i] = surfaces[2*i + 1] = {
            normalize(cross(q.u, q.v)), make_float3(q.color.x, q.color.y, q.color.z) / 255.0f, q.emission
        };
    }

    check(cudaMalloc(&geometry.vertices, sizeof(vertices)));
    check(cudaMemcpy(geometry.vertices, vertices, sizeof(vertices), cudaMemcpyHostToDevice));
    check(cudaMalloc(&geometry.surfaces, sizeof(surfaces)));
    check(cudaMemcpy(geometry.surfaces, surfaces, sizeof(surfaces), cudaMemcpyHostToDevice));

    CUdeviceptr vertex_buffer = reinterpret_cast<CUdeviceptr>(geometry.vertices);
    unsigned flags = OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT;
    OptixBuildInput input {};
    input.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
    input.triangleArray.vertexBuffers = &vertex_buffer;
    input.triangleArray.numVertices = quadCount * 6;
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

Pipeline makePipeline(OptixDeviceContext context, const std::string& ptx) {
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
               const TriangleMesh& geometry, const Camera& camera, const PointLight& light,
               int width, int height) {
    Parameters parameters {};
    parameters.scene = geometry.handle;
    parameters.surfaces = geometry.surfaces;
    parameters.camera = camera;
    parameters.light = light;
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

int main(int argc, char** argv) {
    const int width = 640, height = 640;
    const char* output = argc == 2 ? argv[1] : "build/optix-cornell.ppm";
    std::string ptxName = std::string(argv[0]) + ".ptx";

    std::string ptx = readFile(ptxName);

    check(cudaFree(nullptr));
    check(optixInit());
    OptixDeviceContext context;
    check(optixDeviceContextCreate(nullptr, nullptr, &context));
    std::cout << "Hello, OptiX!\n";

    TriangleMesh cornellBoxMesh = makeCornellBox(context);
    Pipeline pipeline = makePipeline(context, ptx);

    ShaderBindingTable table = makeShaderBindingTable(pipeline);

    Camera camera = makeCamera({278, 278, -800}, {278, 278, 278}, {0, 1, 0}, 40);
    PointLight light {{278, 553.5f, 279.5f}, {100000, 100000, 100000}};

    uchar3* pixels = render(pipeline, table, cornellBoxMesh, camera, light, width, height);
    saveImage(output, pixels, width, height);

    check(cudaFree(pixels));
    check(cudaFree(table.records));

    check(optixPipelineDestroy(pipeline.handle));
    for (auto group : pipeline.groups) {
        check(optixProgramGroupDestroy(group));
    }
    check(optixModuleDestroy(pipeline.module));

    check(cudaFree(cornellBoxMesh.accelerationBuffer));
    check(cudaFree(cornellBoxMesh.vertices));
    check(cudaFree(cornellBoxMesh.surfaces));

    check(optixDeviceContextDestroy(context));
}
