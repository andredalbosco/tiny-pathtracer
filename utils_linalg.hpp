#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>

struct Float2 {
    float x, y;

    constexpr Float2(float x = 0, float y = 0) : x(x), y(y) {}

    float& operator[](std::size_t index) {
        assert(index < 2);
        if (index == 0) return x;
        return y;
    }

    const float& operator[](std::size_t index) const {
        assert(index < 2);
        if (index == 0) return x;
        return y;
    }

    constexpr Float2 operator+() const { return *this; }
    constexpr Float2 operator-() const { return {-x, -y}; }

    constexpr Float2 operator+(Float2 b) const { return {x + b.x, y + b.y}; }
    constexpr Float2 operator-(Float2 b) const { return {x - b.x, y - b.y}; }
    constexpr Float2 operator*(Float2 b) const { return {x * b.x, y * b.y}; }
    constexpr Float2 operator/(Float2 b) const { return {x / b.x, y / b.y}; }
    constexpr Float2 operator*(float scale) const { return {x * scale, y * scale}; }
    constexpr Float2 operator/(float scale) const { return {x / scale, y / scale}; }

    Float2& operator+=(Float2 b) { *this = *this + b; return *this; }
    Float2& operator-=(Float2 b) { *this = *this - b; return *this; }
    Float2& operator*=(Float2 b) { *this = *this * b; return *this; }
    Float2& operator/=(Float2 b) { *this = *this / b; return *this; }
    Float2& operator*=(float scale) { *this = *this * scale; return *this; }
    Float2& operator/=(float scale) { *this = *this / scale; return *this; }
};

constexpr Float2 operator*(float scale, Float2 v) { return v * scale; }
constexpr Float2 operator/(float scale, Float2 v) { return {scale / v.x, scale / v.y}; }

constexpr bool operator==(Float2 a, Float2 b) {
    return a.x == b.x && a.y == b.y;
}
constexpr bool operator!=(Float2 a, Float2 b) { return !(a == b); }

constexpr float dot(Float2 a, Float2 b) {
    return a.x * b.x + a.y * b.y;
}

constexpr float length_squared(Float2 v) { return dot(v, v); }
inline float length(Float2 v) { return std::sqrt(length_squared(v)); }
inline float distance(Float2 a, Float2 b) { return length(a - b); }

// Requires a nonzero vector.
inline Float2 normalize(Float2 v) { return v * (1 / length(v)); }

inline Float2 min(Float2 a, Float2 b) {
    return {std::min(a.x, b.x), std::min(a.y, b.y)};
}

inline Float2 max(Float2 a, Float2 b) {
    return {std::max(a.x, b.x), std::max(a.y, b.y)};
}

inline Float2 abs(Float2 v) {
    return {std::abs(v.x), std::abs(v.y)};
}

// Requires lower <= upper in every component.
inline Float2 clamp(Float2 v, Float2 lower, Float2 upper) {
    return min(max(v, lower), upper);
}

inline Float2 clamp(Float2 v, float lower, float upper) {
    return {std::clamp(v.x, lower, upper), std::clamp(v.y, lower, upper)};
}

constexpr Float2 lerp(Float2 a, Float2 b, float t) {
    return a * (1 - t) + b * t;
}

// Requires a unit normal.
constexpr Float2 reflect(Float2 direction, Float2 normal) {
    return direction - normal * (2 * dot(direction, normal));
}

struct Float3 {
    float x, y, z;

    constexpr Float3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}

    float& operator[](std::size_t index) {
        assert(index < 3);
        if (index == 0) return x;
        if (index == 1) return y;
        return z;
    }

    const float& operator[](std::size_t index) const {
        assert(index < 3);
        if (index == 0) return x;
        if (index == 1) return y;
        return z;
    }

    constexpr Float3 operator+() const { return *this; }
    constexpr Float3 operator-() const { return {-x, -y, -z}; }

    constexpr Float3 operator+(Float3 b) const { return {x + b.x, y + b.y, z + b.z}; }
    constexpr Float3 operator-(Float3 b) const { return {x - b.x, y - b.y, z - b.z}; }
    constexpr Float3 operator*(Float3 b) const { return {x * b.x, y * b.y, z * b.z}; }
    constexpr Float3 operator/(Float3 b) const { return {x / b.x, y / b.y, z / b.z}; }
    constexpr Float3 operator*(float scale) const { return {x * scale, y * scale, z * scale}; }
    constexpr Float3 operator/(float scale) const { return {x / scale, y / scale, z / scale}; }

    Float3& operator+=(Float3 b) { *this = *this + b; return *this; }
    Float3& operator-=(Float3 b) { *this = *this - b; return *this; }
    Float3& operator*=(Float3 b) { *this = *this * b; return *this; }
    Float3& operator/=(Float3 b) { *this = *this / b; return *this; }
    Float3& operator*=(float scale) { *this = *this * scale; return *this; }
    Float3& operator/=(float scale) { *this = *this / scale; return *this; }
};

constexpr Float3 operator*(float scale, Float3 v) { return v * scale; }
constexpr Float3 operator/(float scale, Float3 v) { return {scale / v.x, scale / v.y, scale / v.z}; }

constexpr bool operator==(Float3 a, Float3 b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}
constexpr bool operator!=(Float3 a, Float3 b) { return !(a == b); }

constexpr float dot(Float3 a, Float3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

constexpr float length_squared(Float3 v) { return dot(v, v); }
inline float length(Float3 v) { return std::sqrt(length_squared(v)); }
inline float distance(Float3 a, Float3 b) { return length(a - b); }

// Requires a nonzero vector.
inline Float3 normalize(Float3 v) { return v * (1 / length(v)); }

inline Float3 min(Float3 a, Float3 b) {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}

inline Float3 max(Float3 a, Float3 b) {
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

inline Float3 abs(Float3 v) {
    return {std::abs(v.x), std::abs(v.y), std::abs(v.z)};
}

// Requires lower <= upper in every component.
inline Float3 clamp(Float3 v, Float3 lower, Float3 upper) {
    return min(max(v, lower), upper);
}

inline Float3 clamp(Float3 v, float lower, float upper) {
    return {std::clamp(v.x, lower, upper), std::clamp(v.y, lower, upper), std::clamp(v.z, lower, upper)};
}

constexpr Float3 lerp(Float3 a, Float3 b, float t) {
    return a * (1 - t) + b * t;
}

// Requires a unit normal.
constexpr Float3 reflect(Float3 direction, Float3 normal) {
    return direction - normal * (2 * dot(direction, normal));
}

constexpr Float3 cross(Float3 a, Float3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}
