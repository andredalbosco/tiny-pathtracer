#pragma once
#include "renderer_common.h"
#include <array>

struct Float3Array {
    std::vector<float> x, y, z;
    void resize(int n) { x.resize(n); y.resize(n); z.resize(n); }
    Float3 operator[](int i) const { return {x[i], y[i], z[i]}; }
    void set(int i, Float3 v) { x[i] = v.x; y[i] = v.y; z[i] = v.z; }
};

struct FlatScene : SceneData {
    Float3Array p, e1, e2;
    struct Scratch {
        std::vector<float> t, u, v;
        explicit Scratch(std::size_t n) : t(n), u(n), v(n) {}
    };
    explicit FlatScene(SceneData data) : SceneData(std::move(data)) {}
    void prepare() {
        int n = int(triangles.size());
        p.resize(n); e1.resize(n); e2.resize(n);
        for (int i = 0; i < n; ++i) {
            p.set(i, triangles[i].p);
            e1.set(i, triangles[i].e1);
            e2.set(i, triangles[i].e2);
        }
    }
    void candidates(Ray ray, int count, float* __restrict__ ts,
                    float* __restrict__ us, float* __restrict__ vs) const {
        for (int i = 0; i < count; ++i) {
            Float3 a = e1[i], b = e2[i];
            Float3 h = cross(ray.direction, b);
            float determinant = dot(a, h);
            float inverse = 1 / std::max(determinant, 1e-10f);
            Float3 offset = ray.origin - p[i];
            float u = dot(offset, h) * inverse;
            Float3 q = cross(offset, a);
            float v = dot(ray.direction, q) * inverse;
            float t = dot(b, q) * inverse;
            bool valid = (determinant >= 1e-10f) & (u >= 0) & (u <= 1)
                       & (v >= 0) & (u + v <= 1) & (t > 1e-4f);
            ts[i] = valid ? t : INFINITY;
            us[i] = u;
            vs[i] = v;
        }
    }
    bool intersect(Ray ray, Hit& hit, Scratch& scratch) const {
        candidates(ray, int(triangles.size()), scratch.t.data(), scratch.u.data(), scratch.v.data());
        hit = {};
        for (int i = 0; i < int(triangles.size()); ++i) {
            if (scratch.t[i] < hit.t) hit = {scratch.t[i], scratch.u[i], scratch.v[i], i};
        }
        return hit.triangle >= 0;
    }
};

// Ordinary C++ over several rays. Geometry is loaded once for the ray group.
struct RayScene : ScalarScene {
    using ScalarScene::ScalarScene;
    void intersect_batch(const Ray* rays, Hit* hits, int count, Scratch&) const {
        float ox[8]{}, oy[8]{}, oz[8]{}, dx[8]{}, dy[8]{}, dz[8]{};
        float ts[8], us[8]{}, vs[8]{};
        int ids[8];
        for (int r = 0; r < count; ++r) {
            ox[r] = rays[r].origin.x; oy[r] = rays[r].origin.y; oz[r] = rays[r].origin.z;
            dx[r] = rays[r].direction.x; dy[r] = rays[r].direction.y; dz[r] = rays[r].direction.z;
            ts[r] = INFINITY; ids[r] = -1;
        }
        for (int i = 0; i < int(triangles.size()); ++i) {
            const Triangle& triangle = triangles[i];
            for (int r = 0; r < count; ++r) {
                Float3 direction{dx[r], dy[r], dz[r]};
                Float3 h = cross(direction, triangle.e2);
                float determinant = dot(triangle.e1, h);
                float inverse = 1 / std::max(determinant, 1e-10f);
                Float3 offset = Float3{ox[r], oy[r], oz[r]} - triangle.p;
                float u = dot(offset, h) * inverse;
                Float3 q = cross(offset, triangle.e1);
                float v = dot(direction, q) * inverse;
                float t = dot(triangle.e2, q) * inverse;
                bool valid = (determinant >= 1e-10f) & (u >= 0) & (u <= 1)
                           & (v >= 0) & (u + v <= 1) & (t > 1e-4f) & (t < ts[r]);
                ts[r] = valid ? t : ts[r];
                us[r] = valid ? u : us[r];
                vs[r] = valid ? v : vs[r];
                ids[r] = valid ? i : ids[r];
            }
        }
        for (int r = 0; r < count; ++r) hits[r] = {ts[r], us[r], vs[r], ids[r]};
    }
};
