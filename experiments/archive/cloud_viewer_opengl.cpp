// One-file WDAS cloud viewer: C++17, NanoVDB loading, raylib, embedded GLSL.
// Build with: make cloud-viewer
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <raylib.h>
#include <rlgl.h>
#include <nanovdb/util/IO.h>
#include <nanovdb/util/SampleFromVoxels.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

const char* fragmentShader = R"GLSL(#version 330
uniform sampler3D cloud;
uniform vec3 halfBounds, cameraPosition, sunDirection;
uniform vec2 resolution;
uniform float extinction, exposure;
uniform int steps, shadowSteps;
out vec4 finalColor;

vec2 intersectBounds(vec3 origin, vec3 direction) {
    float enter = 0.0, exit = 1e20;
    for (int axis = 0; axis < 3; ++axis) {
        if (abs(direction[axis]) < 1e-8) {
            if (abs(origin[axis]) > halfBounds[axis]) return vec2(1, 0);
        } else {
            float a = (-halfBounds[axis]-origin[axis])/direction[axis];
            float b = ( halfBounds[axis]-origin[axis])/direction[axis];
            enter = max(enter, min(a, b));
            exit = min(exit, max(a, b));
        }
    }
    return vec2(enter, exit);
}

float density(vec3 p) {
    vec3 uv = p/(2.0*halfBounds)+0.5;
    if (any(lessThan(uv, vec3(0))) || any(greaterThan(uv, vec3(1)))) return 0.0;
    return max(0.0, texture(cloud, uv).r);
}

float sunlight(vec3 p) {
    vec2 interval = intersectBounds(p, sunDirection);
    float stepLength = max(0.0, interval.y-interval.x)/float(shadowSteps);
    float opticalDepth = 0.0;
    for (int i = 0; i < shadowSteps; ++i) {
        opticalDepth += density(p+sunDirection*(interval.x+(float(i)+0.5)*stepLength))*extinction*stepLength;
        if (opticalDepth > 12.0) break;
    }
    return exp(-opticalDepth);
}

void main() {
    vec2 clip = 2.0*gl_FragCoord.xy/resolution-1.0;
    vec3 forward = normalize(-cameraPosition);
    vec3 right = normalize(cross(forward, vec3(0, 1, 0)));
    vec3 up = cross(right, forward);
    vec3 direction = normalize(forward+0.414213562*(right*clip.x*resolution.x/resolution.y+up*clip.y));
    vec3 background = mix(vec3(0.12, 0.21, 0.38), vec3(0.65, 0.78, 0.95),
                          clamp(0.5+0.5*direction.y, 0.0, 1.0));
    vec2 interval = intersectBounds(cameraPosition, direction);
    float stepLength = max(0.0, interval.y-interval.x)/float(steps);
    float transmittance = 1.0;
    vec3 color = vec3(0);
    const float g = 0.35;
    float mu = dot(direction, sunDirection);
    float phase = (1.0-g*g)/(12.566370614*pow(1.0+g*g-2.0*g*mu, 1.5));

    if (interval.y > interval.x && extinction > 0.0) {
        for (int i = 0; i < steps; ++i) {
            vec3 p = cameraPosition+direction*(interval.x+(float(i)+0.5)*stepLength);
            float sigma = density(p)*extinction;
            float attenuation = exp(-sigma*stepLength);
            if (sigma > 1e-5) {
                vec3 source = 0.95*(vec3(30, 28, 25)*phase*sunlight(p)+vec3(0.07, 0.09, 0.12));
                color += transmittance*(1.0-attenuation)*source;
            }
            transmittance *= attenuation;
            if (transmittance < 1e-5) break;
        }
    }
    color = max(vec3(0), (color+transmittance*background)*exp2(exposure));
    vec3 srgb = mix(12.92*color, 1.055*pow(color, vec3(1.0/2.4))-0.055,
                    step(vec3(0.0031308), color));
    finalColor = vec4(clamp(srgb, 0.0, 1.0), 1);
}
)GLSL";

unsigned loadCloud(const char* filename, Vector3& halfBounds) {
    auto handle = nanovdb::io::readGrid(filename, std::string("density"));
    const auto* grid = handle.grid<float>();
    if (!grid || !grid->isFogVolume() || grid->activeVoxelCount() == 0 || grid->tree().background() != 0)
        throw std::runtime_error("Expected a nonempty, zero-background float fog grid named density");

    auto bounds = grid->worldBBox();
    auto extent = bounds.max()-bounds.min();
    double longest = std::max({extent[0], extent[1], extent[2]});
    if (!std::isfinite(longest) || longest <= 0)
        throw std::runtime_error("Expected finite, nonempty cloud bounds");
    halfBounds = {float(extent[0]/longest), float(extent[1]/longest), float(extent[2]/longest)};

    // Resample once into a small, world-aligned 3D texture, longest axis 128.
    int nx = std::max(2, int(std::ceil(128*extent[0]/longest)));
    int ny = std::max(2, int(std::ceil(128*extent[1]/longest)));
    int nz = std::max(2, int(std::ceil(128*extent[2]/longest)));
    std::vector<float> voxels(size_t(nx)*ny*nz);
    auto accessor = grid->getAccessor();
    auto sample = nanovdb::createSampler<1>(accessor);
    for (int z = 0; z < nz; ++z)
        for (int y = 0; y < ny; ++y)
            for (int x = 0; x < nx; ++x) {
                nanovdb::Vec3f world(bounds.min()[0]+(x+0.5)*extent[0]/nx,
                                     bounds.min()[1]+(y+0.5)*extent[1]/ny,
                                     bounds.min()[2]+(z+0.5)*extent[2]/nz);
                voxels[(size_t(z)*ny+y)*nx+x] = std::max(0.0f, sample(grid->worldToIndexF(world)));
            }

    unsigned texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_3D, texture);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, nx, ny, nz, 0, GL_RED, GL_FLOAT, voxels.data());
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Cannot upload cloud 3D texture");
    glBindTexture(GL_TEXTURE_3D, 0);
    std::cout << "Loaded " << filename << "; density texture " << nx << 'x' << ny << 'x' << nz
              << " (" << voxels.size()*sizeof(float) << " bytes)\n";
    return texture;
}

void saveScreenshot() {
    Image image = LoadImageFromScreen();
    bool saved = ExportImage(image, "build/cloud-viewer.png");
    UnloadImage(image);
    if (!saved) throw std::runtime_error("Cannot save build/cloud-viewer.png");
}

int main(int argc, char** argv) try {
    const char* filename = "../volume-assets/wdas-cloud/wdas_cloud_eighth.nvdb";
    bool smoke = false, haveFilename = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [cloud.nvdb] [--smoke]\n";
            return 0;
        } else if (arg == "--smoke") smoke = true;
        else if (!haveFilename && arg.rfind("--", 0) != 0) { filename = argv[i]; haveFilename = true; }
        else throw std::runtime_error("Usage: cloud_viewer [cloud.nvdb] [--smoke]");
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(960, 720, "WDAS Cloud Viewer");
    if (!IsWindowReady()) throw std::runtime_error("Cannot open viewer window");
    SetWindowMinSize(800, 600);
    SetTargetFPS(60);
    BeginDrawing(); ClearBackground(BLACK); DrawText("Loading WDAS cloud...", 24, 24, 24, WHITE); EndDrawing();

    Vector3 halfBounds;
    unsigned cloud = loadCloud(filename, halfBounds);
    Shader shader = LoadShaderFromMemory(nullptr, fragmentShader);
    if (!IsShaderValid(shader) || shader.id == rlGetShaderIdDefault())
        throw std::runtime_error("Cannot compile cloud shader");
    int textureSlot = 1;
    SetShaderValue(shader, GetShaderLocation(shader, "cloud"), &textureSlot, SHADER_UNIFORM_INT);
    SetShaderValue(shader, GetShaderLocation(shader, "halfBounds"), &halfBounds, SHADER_UNIFORM_VEC3);

    RenderTexture2D target {};
    float yaw = 0, pitch = std::atan2(0.32f, 3.2f), distance = std::hypot(3.2f, 0.32f);
    float sunYaw = 2.446854f, sunPitch = 0.797f, extinction = 4, exposure = -1;
    bool refined = true, dirty = true;
    int frames = 0;
    while (!WindowShouldClose()) {
        Vector2 delta = GetMouseDelta();
        float wheel = GetMouseWheelMove();
        bool drag = (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
                    && (delta.x != 0 || delta.y != 0);
        bool adjusting = IsKeyDown(KEY_LEFT_BRACKET) || IsKeyDown(KEY_RIGHT_BRACKET)
                         || IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_EQUAL);
        bool moving = drag || wheel != 0 || adjusting;
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && drag) {
            yaw -= delta.x*0.005f;
            pitch = std::clamp(pitch+delta.y*0.005f, -1.45f, 1.45f);
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && drag) {
            sunYaw -= delta.x*0.005f;
            sunPitch = std::clamp(sunPitch+delta.y*0.005f, -0.1f, 1.5f);
        }
        if (wheel != 0) distance = std::clamp(distance*std::exp(-wheel*0.1f), 1.4f, 12.0f);
        float dt = std::min(GetFrameTime(), 0.05f);
        extinction = std::clamp(extinction*std::exp(dt*(IsKeyDown(KEY_RIGHT_BRACKET)-IsKeyDown(KEY_LEFT_BRACKET))), 0.05f, 40.0f);
        exposure = std::clamp(exposure+2*dt*(IsKeyDown(KEY_EQUAL)-IsKeyDown(KEY_MINUS)), -6.0f, 4.0f);
        if (IsKeyPressed(KEY_SPACE)) { refined = !refined; dirty = true; }
        if (IsKeyPressed(KEY_R)) {
            yaw = 0; pitch = std::atan2(0.32f, 3.2f); distance = std::hypot(3.2f, 0.32f);
            sunYaw = 2.446854f; sunPitch = 0.797f; extinction = 4; exposure = -1;
            dirty = true;
        }

        int width = smoke ? 160 : (refined && !moving ? 480 : 240);
        int height = std::max(1, width*GetScreenHeight()/GetScreenWidth());
        if (target.texture.width != width || target.texture.height != height) {
            if (target.id) UnloadRenderTexture(target);
            target = LoadRenderTexture(width, height);
            if (!IsRenderTextureValid(target)) throw std::runtime_error("Cannot create render target");
            SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
            dirty = true;
        }
        if (dirty || moving) {
            Vector3 camera {distance*std::cos(pitch)*std::cos(yaw), distance*std::sin(pitch),
                            distance*std::cos(pitch)*std::sin(yaw)};
            Vector3 sun {std::cos(sunPitch)*std::cos(sunYaw), std::sin(sunPitch),
                         std::cos(sunPitch)*std::sin(sunYaw)};
            Vector2 resolution {float(width), float(height)};
            int steps = smoke ? 64 : (refined && !moving ? 192 : 96);
            int shadows = smoke ? 16 : (refined && !moving ? 32 : 16);
            SetShaderValue(shader, GetShaderLocation(shader, "cameraPosition"), &camera, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, GetShaderLocation(shader, "sunDirection"), &sun, SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, GetShaderLocation(shader, "resolution"), &resolution, SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, GetShaderLocation(shader, "extinction"), &extinction, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, GetShaderLocation(shader, "exposure"), &exposure, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, GetShaderLocation(shader, "steps"), &steps, SHADER_UNIFORM_INT);
            SetShaderValue(shader, GetShaderLocation(shader, "shadowSteps"), &shadows, SHADER_UNIFORM_INT);
            BeginTextureMode(target);
            BeginShaderMode(shader);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_3D, cloud);
            glActiveTexture(GL_TEXTURE0);
            DrawRectangle(0, 0, width, height, WHITE);
            EndShaderMode();
            EndTextureMode();
            dirty = false;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(target.texture, {0, 0, float(width), -float(height)},
                       {0, 0, float(GetScreenWidth()), float(GetScreenHeight())}, {0, 0}, 0, WHITE);
        DrawRectangle(0, 0, GetScreenWidth(), 42, Fade(BLACK, 0.75f));
        DrawText("WDAS CLOUD", 14, 11, 20, WHITE);
        DrawText(TextFormat("Density %.2f   Exposure %+.1f EV   %dx%d", extinction, exposure, width, height), 180, 14, 16, LIGHTGRAY);
        DrawFPS(GetScreenWidth()-90, 11);
        DrawRectangle(0, GetScreenHeight()-60, GetScreenWidth(), 60, Fade(BLACK, 0.75f));
        DrawText("Left drag: orbit   Wheel: zoom   Right drag: sun   R: reset", 14, GetScreenHeight()-48, 16, WHITE);
        DrawText("[ / ]: density   - / =: exposure   Space: quality   S: save   Esc: close", 14, GetScreenHeight()-25, 16, WHITE);
        EndDrawing();
        if (IsKeyPressed(KEY_S) || (smoke && frames == 2)) saveScreenshot();
        if (smoke && ++frames >= 3) break;
    }
    UnloadRenderTexture(target);
    UnloadShader(shader);
    glDeleteTextures(1, &cloud);
    CloseWindow();
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    if (IsWindowReady()) CloseWindow();
    return 1;
}
