#include "nvdb_renderer.h"
#include <nanovdb/util/IO.h>
#include <raylib.h>
#include <algorithm>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [cloud.nvdb]\n";
        return 1;
    }

    const int width = 640, height = 640;
    const char* filename = argc > 1 ? argv[1] : "../volume-assets/wdas-cloud/wdas_cloud_quarter.nvdb";
    auto handle = nanovdb::io::readGrid(filename, std::string("density"));
    const auto* grid = handle.grid<float>();
    if (!grid || !grid->isFogVolume() || grid->activeVoxelCount() == 0 || grid->tree().background() != 0) {
        std::cerr << "Expected a nonempty, zero-background float fog grid named density\n";
        return 1;
    }

    auto bounds = grid->worldBBox();
    auto center = (bounds.min() + bounds.max()) * 0.5;
    auto extent = bounds.max() - bounds.min();
    float scale = float(std::max({extent[0], extent[1], extent[2]})) * 0.5f;
    nvdb::Vec3 target(center[0], center[1], center[2]);
    float yaw = 0;
    float pitch = std::atan2(0.32f, 3.2f);
    float distance = nvdb::Vec3(3.2f, 0.32f, 0).length() * scale;
    nvdb::DebugMode mode = nvdb::DebugMode::Silhouette;
    const char* modeNames[] = {"Silhouette", "Max density", "First-hit depth", "Traversal cells"};

    nvdb::Camera camera = nvdb::makeCamera(target + nvdb::Vec3(3.2f, 0.32f, 0) * scale,
                                           target, {0, 1, 0}, 40);
    auto pixels = nvdb::render(*grid, camera, width, height);

    InitWindow(width, height, "NanoVDB Viewer");
    if (!IsWindowReady()) return 1;

    Image image {};
    image.data = pixels.data();
    image.width = width;
    image.height = height;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
    Texture2D texture = LoadTextureFromImage(image);
    if (!texture.id) {
        CloseWindow();
        return 1;
    }
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        bool changed = false;
        bool cameraChanged = false;
        Vector2 delta = GetMouseDelta();
        float wheel = GetMouseWheelMove();

        for (int i = 0; i < 4; ++i) {
            if (IsKeyPressed(KEY_ONE + i) && mode != static_cast<nvdb::DebugMode>(i)) {
                mode = static_cast<nvdb::DebugMode>(i);
                changed = true;
            }
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && (delta.x != 0 || delta.y != 0)) {
            yaw -= delta.x * 0.005f;
            pitch = std::clamp(pitch + delta.y * 0.005f, -1.5f, 1.5f);
            changed = true;
            cameraChanged = true;
        }

        if (wheel != 0) {
            distance = std::clamp(distance * std::exp(-wheel * 0.1f), scale * 0.2f, scale * 100);
            changed = true;
            cameraChanged = true;
        }

        if (cameraChanged) {
            nvdb::Vec3 offset(std::cos(pitch) * std::cos(yaw), std::sin(pitch),
                              std::cos(pitch) * std::sin(yaw));
            camera = nvdb::makeCamera(target + offset * distance, target, {0, 1, 0}, 40);
        }

        if (changed) {
            pixels = nvdb::render(*grid, camera, width, height, mode);
            UpdateTexture(texture, pixels.data());
        }

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexture(texture, 0, 0, WHITE);
        DrawRectangle(0, 0, width, 28, Fade(BLACK, 0.75f));
        DrawText(modeNames[static_cast<int>(mode)], 10, 6, 16, WHITE);
        DrawRectangle(0, height - 52, width, 52, Fade(BLACK, 0.75f));
        DrawText("1 Silhouette | 2 Density | 3 Depth | 4 Traversal", 10, height - 44, 16, WHITE);
        DrawText("Drag to orbit | Wheel to zoom | Esc to close", 10, height - 20, 16, WHITE);
        EndDrawing();
    }

    UnloadTexture(texture);
    CloseWindow();
}
