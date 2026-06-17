#pragma once

// ============================================================================
// Math.h — Common math types and helpers.
//
// Phase 1: Basic Vec3f, quaternion, and fixed-point helpers.
// Phase 2+: Eigen integration via thirdparty submodule.
// ============================================================================

#include <cmath>
#include <array>

namespace common::math {

// ---------------------------------------------------------------------------
// Vec3f — trivially copyable 3D vector (RT-safe, no allocation).
// ---------------------------------------------------------------------------
struct Vec3f {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    [[nodiscard]] float magnitude() const noexcept {
        return static_cast<float>(std::sqrt(static_cast<double>(x) * x + static_cast<double>(y) * y + static_cast<double>(z) * z));
    }

    [[nodiscard]] Vec3f operator+(const Vec3f& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z};
    }

    [[nodiscard]] Vec3f operator-(const Vec3f& o) const noexcept {
        return {x - o.x, y - o.y, z - o.z};
    }

    [[nodiscard]] Vec3f operator*(float s) const noexcept {
        return {x * s, y * s, z * s};
    }
};

// ---------------------------------------------------------------------------
// Fixed-point helpers (20-bit fractional).
// Used for sub-cycle precision in simulated encoder physics.
// ---------------------------------------------------------------------------
inline constexpr int FixedShift = 20;
inline constexpr int64_t FixedOne = 1LL << FixedShift;

[[nodiscard]] inline int64_t toFixed(float v) noexcept {
    return static_cast<int64_t>(v * static_cast<float>(FixedOne));
}

[[nodiscard]] inline float fromFixed(int64_t v) noexcept {
    return static_cast<float>(v) / static_cast<float>(FixedOne);
}

} // namespace common::math
