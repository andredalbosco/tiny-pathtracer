#pragma once

#include <cmath>

struct Float2 {
    float x, y;

    Float2(float x = 0, float y = 0) : x(x), y(y) {}
    Float2 operator+(Float2 b) const { return {x + b.x, y + b.y}; }
    Float2 operator-(Float2 b) const { return {x - b.x, y - b.y}; }
    Float2 operator*(Float2 b) const { return {x * b.x, y * b.y}; }
    Float2 operator*(float scale) const { return {x * scale, y * scale}; }
};

struct Float3 {
    float x, y, z;

    Float3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
    Float3 operator+(Float3 b) const { return {x + b.x, y + b.y, z + b.z}; }
    Float3 operator-(Float3 b) const { return {x - b.x, y - b.y, z - b.z}; }
    Float3 operator*(Float3 b) const { return {x * b.x, y * b.y, z * b.z}; }
    Float3 operator*(float scale) const { return {x * scale, y * scale, z * scale}; }
    Float3 operator+=(Float3 b) { *this = *this + b; return *this; }
    Float3 operator*=(Float3 b) { *this = *this * b; return *this; }
};

inline float dot(Float3 a, Float3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Float3 cross(Float3 a, Float3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline Float3 normalize(Float3 v) {
    return v * (1 / std::sqrt(dot(v, v)));
}
