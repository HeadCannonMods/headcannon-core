#pragma once

#include <cmath>
#include "cameraunlock/math/vec3.h"

namespace cameraunlock {
namespace math {

/// Immutable-style quaternion for rotation representation (xyzw component order).
/// Port of CameraUnlock.Core.Data.Quat4 (C#).
struct Quat4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    Quat4() = default;
    Quat4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    static Quat4 Identity() { return Quat4(0.0f, 0.0f, 0.0f, 1.0f); }

    Quat4 Negated() const { return Quat4(-x, -y, -z, -w); }

    /// Returns the conjugate/inverse of a unit quaternion.
    Quat4 Inverse() const { return Quat4(-x, -y, -z, w); }

    float Dot(const Quat4& other) const {
        return x * other.x + y * other.y + z * other.z + w * other.w;
    }

    /// Rotates a vector by this quaternion: q * v * q^-1 (optimized).
    Vec3 Rotate(const Vec3& v) const {
        float x2 = x + x;
        float y2 = y + y;
        float z2 = z + z;

        float xx2 = x * x2;
        float yy2 = y * y2;
        float zz2 = z * z2;
        float xy2 = x * y2;
        float xz2 = x * z2;
        float yz2 = y * z2;
        float wx2 = w * x2;
        float wy2 = w * y2;
        float wz2 = w * z2;

        return Vec3(
            (1.0f - yy2 - zz2) * v.x + (xy2 - wz2) * v.y + (xz2 + wy2) * v.z,
            (xy2 + wz2) * v.x + (1.0f - xx2 - zz2) * v.y + (yz2 - wx2) * v.z,
            (xz2 - wy2) * v.x + (yz2 + wx2) * v.y + (1.0f - xx2 - yy2) * v.z
        );
    }

    /// Multiplies two quaternions: this * b.
    Quat4 Multiply(const Quat4& b) const {
        return Quat4(
            w * b.x + x * b.w + y * b.z - z * b.y,
            w * b.y - x * b.z + y * b.w + z * b.x,
            w * b.z + x * b.y - y * b.x + z * b.w,
            w * b.w - x * b.x - y * b.y - z * b.z
        );
    }

    Quat4 operator*(const Quat4& b) const { return Multiply(b); }

    /// Creates a quaternion from YXZ Euler angles (yaw, pitch, roll in degrees).
    /// Matches C# QuaternionUtils.FromYawPitchRoll.
    static Quat4 FromYawPitchRoll(float yawDeg, float pitchDeg, float rollDeg) {
        constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
        float halfYaw = yawDeg * kDegToRad * 0.5f;
        float halfPitch = pitchDeg * kDegToRad * 0.5f;
        float halfRoll = rollDeg * kDegToRad * 0.5f;

        float sy = std::sin(halfYaw);
        float cy = std::cos(halfYaw);
        float sp = std::sin(halfPitch);
        float cp = std::cos(halfPitch);
        float sr = std::sin(halfRoll);
        float cr = std::cos(halfRoll);

        return Quat4(
            cy * sp * cr + sy * cp * sr,
            sy * cp * cr - cy * sp * sr,
            cy * cp * sr - sy * sp * cr,
            cy * cp * cr + sy * sp * sr
        );
    }
};

}  // namespace math
}  // namespace cameraunlock
