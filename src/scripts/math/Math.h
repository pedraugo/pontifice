#pragma once

#include <cmath>

namespace math {

constexpr float kPi = 3.14159265358979323846f;

inline float Radians(float degrees) { return degrees * kPi / 180.0f; }
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct Vec2 {
  float x = 0, y = 0;
};

// Any winding; used for mouse hit tests on projected cards and slots.
inline bool PointInConvexQuad(Vec2 p, const Vec2 q[4]) {
  float sign = 0.0f;
  for (int i = 0; i < 4; ++i) {
    const Vec2 a = q[i];
    const Vec2 b = q[(i + 1) % 4];
    const float cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
    if (cross < 1e-6f && cross > -1e-6f) continue;
    if (sign == 0.0f) {
      sign = cross;
    } else if ((cross > 0) != (sign > 0)) {
      return false;
    }
  }
  return sign != 0.0f;
}

struct Vec3 {
  float x = 0, y = 0, z = 0;

  Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
};

inline float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 Cross(const Vec3& a, const Vec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline Vec3 Normalize(const Vec3& v) {
  const float len = std::sqrt(Dot(v, v));
  return len > 0 ? v * (1.0f / len) : v;
}

struct Vec4 {
  float x = 0, y = 0, z = 0, w = 0;
};

// Column-major, as in Metal and GLSL; translation lives in m[3].
struct Mat4 {
  // Indexed m[column][row].
  float m[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};

  static Mat4 Identity() { return Mat4{}; }

  static Mat4 Translate(const Vec3& t) {
    Mat4 r;
    r.m[3][0] = t.x;
    r.m[3][1] = t.y;
    r.m[3][2] = t.z;
    return r;
  }

  static Mat4 Scale(float s) { return Scale({s, s, s}); }

  static Mat4 Scale(const Vec3& s) {
    Mat4 r;
    r.m[0][0] = s.x;
    r.m[1][1] = s.y;
    r.m[2][2] = s.z;
    return r;
  }

  static Mat4 RotateX(float rad) {
    Mat4 r;
    const float c = std::cos(rad), s = std::sin(rad);
    r.m[1][1] = c;
    r.m[1][2] = s;
    r.m[2][1] = -s;
    r.m[2][2] = c;
    return r;
  }

  static Mat4 RotateY(float rad) {
    Mat4 r;
    const float c = std::cos(rad), s = std::sin(rad);
    r.m[0][0] = c;
    r.m[0][2] = -s;
    r.m[2][0] = s;
    r.m[2][2] = c;
    return r;
  }

  static Mat4 RotateZ(float rad) {
    Mat4 r;
    const float c = std::cos(rad), s = std::sin(rad);
    r.m[0][0] = c;
    r.m[0][1] = s;
    r.m[1][0] = -s;
    r.m[1][1] = c;
    return r;
  }

  // Depth maps to [0, 1] (Metal, Vulkan, D3D), not [-1, 1].
  static Mat4 Perspective(float fov_y_rad, float aspect, float near, float far) {
    Mat4 r;
    const float f = 1.0f / std::tan(fov_y_rad / 2.0f);
    r.m[0][0] = f / aspect;
    r.m[1][1] = f;
    r.m[2][2] = far / (near - far);
    r.m[2][3] = -1.0f;
    r.m[3][2] = (near * far) / (near - far);
    r.m[3][3] = 0.0f;
    return r;
  }

  static Mat4 LookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
    const Vec3 f = Normalize(target - eye);
    const Vec3 s = Normalize(Cross(f, up));
    const Vec3 u = Cross(s, f);
    Mat4 r;
    r.m[0][0] = s.x;
    r.m[1][0] = s.y;
    r.m[2][0] = s.z;
    r.m[0][1] = u.x;
    r.m[1][1] = u.y;
    r.m[2][1] = u.z;
    r.m[0][2] = -f.x;
    r.m[1][2] = -f.y;
    r.m[2][2] = -f.z;
    r.m[3][0] = -Dot(s, eye);
    r.m[3][1] = -Dot(u, eye);
    r.m[3][2] = Dot(f, eye);
    return r;
  }

  Mat4 operator*(const Mat4& o) const {
    Mat4 r;
    for (int c = 0; c < 4; ++c) {
      for (int l = 0; l < 4; ++l) {
        r.m[c][l] = m[0][l] * o.m[c][0] + m[1][l] * o.m[c][1] + m[2][l] * o.m[c][2] +
                    m[3][l] * o.m[c][3];
      }
    }
    return r;
  }

  Vec4 operator*(const Vec4& v) const {
    return {m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0] * v.w,
            m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1] * v.w,
            m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2] * v.w,
            m[0][3] * v.x + m[1][3] * v.y + m[2][3] * v.z + m[3][3] * v.w};
  }

  Vec3 TransformPoint(const Vec3& p) const {
    const Vec4 r = (*this) * Vec4{p.x, p.y, p.z, 1.0f};
    return {r.x, r.y, r.z};
  }
};

}
