#include "nvdb_renderer.h"
#include <nanovdb/util/IO.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

void saveImage(const char* filename, const std::vector<nvdb::Pixel>& pixels, int width, int height) {
    std::ofstream image(filename, std::ios::binary);
    image << "P6\n" << width << ' ' << height << "\n255\n";
    image.write(reinterpret_cast<const char*>(pixels.data()), pixels.size() * sizeof(nvdb::Pixel));
    image.close();
    if (!image) {
        std::cerr << "Cannot write " << filename << '\n';
        std::exit(1);
    }

    std::cout << "Saved " << filename << '\n';
}

int main(int argc, char** argv) {
    if (argc > 3) {
        std::cerr << "Usage: " << argv[0] << " [cloud.nvdb] [output.ppm]\n";
        return 1;
    }

    const int width = 640, height = 640;
    const char* filename = argc > 1 ? argv[1] : "../volume-assets/wdas-cloud/wdas_cloud_quarter.nvdb";
    const char* output = argc > 2 ? argv[2] : "build/nvdb-cloud.ppm";
    auto handle = nanovdb::io::readGrid(filename, std::string("density"));
    const auto* grid = handle.grid<float>();
    if (!grid || !grid->isFogVolume() || grid->activeVoxelCount() == 0 || grid->tree().background() != 0) {
        std::cerr << "Expected a nonempty, zero-background float fog grid named density\n";
        return 1;
    }

    std::cout << "Loaded " << filename << " (" << handle.size() << " bytes)\n";

    auto bounds = grid->worldBBox();
    auto center = (bounds.min() + bounds.max()) * 0.5;
    auto extent = bounds.max() - bounds.min();
    float scale = float(std::max({extent[0], extent[1], extent[2]})) * 0.5f;
    nvdb::Vec3 target(center[0], center[1], center[2]);
    nvdb::Camera camera = nvdb::makeCamera(target + nvdb::Vec3(3.2f, 0.32f, 0) * scale,
                                           target, {0, 1, 0}, 40);

    auto pixels = nvdb::render(*grid, camera, width, height);
    saveImage(output, pixels, width, height);
}
