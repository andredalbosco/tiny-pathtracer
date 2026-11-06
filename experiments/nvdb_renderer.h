#pragma once

#include <nanovdb/util/HDDA.h>
#include <nanovdb/util/Ray.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace nvdb {

using Vec3 = nanovdb::Vec3f;
using Ray = nanovdb::Ray<float>;
using Pixel = std::array<unsigned char, 3>;

struct Camera {
    Vec3 position;
    Vec3 forward, right, up;
    float tanHalfFov;
};

enum class DebugMode {
    Silhouette,
    Density,
    Depth,
    Traversal
};

struct Intersection {
    bool found {};
    float t {};
    float maxDensity {};
    unsigned steps {};
};

inline Camera makeCamera(Vec3 position, Vec3 target, Vec3 up, float verticalFov) {
    Camera camera {};
    camera.position = position;
    camera.forward = (target - position).normalize();
    camera.right = up.cross(camera.forward).normalize();
    camera.up = camera.forward.cross(camera.right);
    camera.tanHalfFov = std::tan(verticalFov * M_PI / 360);

    return camera;
}

inline Intersection traceCloud(const nanovdb::FloatGrid& grid, const Ray& worldRay,
                                 bool fullTraversal = false) {
    Intersection intersection {};
    Ray ray(grid.worldToIndexF(worldRay.eye()), grid.worldToIndexDirF(worldRay.dir()),
            worldRay.t0(), worldRay.t1());
    auto accessor = grid.getAccessor();
    if (!ray.clip(accessor.root().bbox())) return intersection;

    auto voxel = nanovdb::RoundDown<nanovdb::Coord>(ray.start());
    nanovdb::HDDA<Ray> hdda(ray, accessor.getDim(voxel, ray));

    for (;;) {
        voxel = hdda.voxel();
        if (hdda.update(ray, accessor.getDim(voxel, ray))) continue;
        ++intersection.steps;
        float density = accessor.getValue(voxel);

        if (density > 0) {
            if (!intersection.found) intersection.t = hdda.time();
            intersection.found = true;
            intersection.maxDensity = std::max(intersection.maxDensity, density);
            if (!fullTraversal) return intersection;
        }

        if (!hdda.step()) return intersection;
    }
}

inline bool intersectCloud(const nanovdb::FloatGrid& grid, const Ray& ray) {
    return traceCloud(grid, ray).found;
}

inline Pixel toPixel(Vec3 color) {
    return {
        static_cast<unsigned char>(255 * std::clamp(color[0], 0.0f, 1.0f)),
        static_cast<unsigned char>(255 * std::clamp(color[1], 0.0f, 1.0f)),
        static_cast<unsigned char>(255 * std::clamp(color[2], 0.0f, 1.0f))
    };
}

inline std::vector<Pixel> render(const nanovdb::FloatGrid& grid, const Camera& camera,
                                  int width, int height, DebugMode mode = DebugMode::Silhouette) {
    std::vector<Pixel> pixels(width * height);
    auto bounds = grid.worldBBox();
    auto center = (bounds.min() + bounds.max()) * 0.5;
    float radius = float((bounds.max() - bounds.min()).length()) * 0.5f;
    float distance = (camera.position - Vec3(center[0], center[1], center[2])).length();
    float depthMin = std::max(0.0f, distance - radius);
    float depthMax = distance + radius;
    float maxDensity = std::max(1e-20f, grid.tree().root().maximum());

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float clipX = 2 * (x + 0.5f) / width - 1;
            float clipY = 1 - 2 * (y + 0.5f) / height;
            float sx = camera.tanHalfFov * float(width) / height * clipX;
            float sy = camera.tanHalfFov * clipY;
            Vec3 direction = (camera.forward + camera.right * sx + camera.up * sy).normalize();

            Ray ray(camera.position, direction, 0.0f, 1e16f);
            Intersection intersection = traceCloud(grid, ray, mode == DebugMode::Density);
            Vec3 color(intersection.found ? 1.0f : 0.0f);

            if (mode == DebugMode::Density)
                color = Vec3(intersection.maxDensity / maxDensity);

            if (mode == DebugMode::Depth && intersection.found)
                color = Vec3(1 - (intersection.t - depthMin) / (depthMax - depthMin));

            if (mode == DebugMode::Traversal && intersection.steps > 0) {
                float cost = std::min(1.0f, std::log2(1.0f + intersection.steps) / 10);
                color = Vec3(cost, 4 * cost * (1 - cost), 1 - cost);
            }

            pixels[y * width + x] = toPixel(color);
        }
    }

    return pixels;
}

}
