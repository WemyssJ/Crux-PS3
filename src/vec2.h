#ifndef CRUX_VEC2_H
#define CRUX_VEC2_H

#include <math.h>

#ifndef CRUX_PI
#define CRUX_PI 3.14159265358979323846f
#endif

// Minimal 2D vector math mirroring the subset of UnityEngine.Vector2 that
// PlayerController.cs / CameraController.cs actually use.
struct Vec2
{
    float x, y;

    Vec2() : x(0.0f), y(0.0f) {}
    Vec2(float _x, float _y) : x(_x), y(_y) {}

    Vec2 operator+(const Vec2 &o) const { return Vec2(x + o.x, y + o.y); }
    Vec2 operator-(const Vec2 &o) const { return Vec2(x - o.x, y - o.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
    Vec2 operator/(float s) const { return Vec2(x / s, y / s); }
    Vec2 &operator+=(const Vec2 &o) { x += o.x; y += o.y; return *this; }
    Vec2 &operator-=(const Vec2 &o) { x -= o.x; y -= o.y; return *this; }
    Vec2 &operator*=(float s) { x *= s; y *= s; return *this; }

    float magnitude() const { return sqrtf(x * x + y * y); }
    float sqrMagnitude() const { return x * x + y * y; }

    Vec2 normalized() const
    {
        float m = magnitude();
        if (m < 1e-8f) return Vec2(0.0f, 0.0f);
        return Vec2(x / m, y / m);
    }
};

inline float Dot(const Vec2 &a, const Vec2 &b) { return a.x * b.x + a.y * b.y; }

inline Vec2 ClampMagnitude(const Vec2 &v, float maxLen)
{
    float m = v.magnitude();
    if (m > maxLen && m > 1e-8f) return v * (maxLen / m);
    return v;
}

// Matches Vector2.SignedAngle(from, to): degrees, positive = counter-clockwise.
inline float SignedAngle(const Vec2 &from, const Vec2 &to)
{
    float unsignedAngle = acosf(fmaxf(-1.0f, fminf(1.0f, Dot(from.normalized(), to.normalized())))) * (180.0f / CRUX_PI);
    float sign = (from.x * to.y - from.y * to.x) < 0.0f ? -1.0f : 1.0f;
    return unsignedAngle * sign;
}

inline float DeltaAngle(float current, float target)
{
    float delta = fmodf(target - current, 360.0f);
    if (delta > 180.0f) delta -= 360.0f;
    if (delta < -180.0f) delta += 360.0f;
    return delta;
}

inline float MoveTowardsAngle(float current, float target, float maxDelta)
{
    float delta = DeltaAngle(current, target);
    if (-maxDelta < delta && delta < maxDelta) return target;
    target = current + delta;
    return (delta > 0.0f) ? fminf(current + maxDelta, target) : fmaxf(current - maxDelta, target);
}

inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float Clamp01(float v) { return Clamp(v, 0.0f, 1.0f); }
inline float Lerp(float a, float b, float t) { return a + (b - a) * Clamp01(t); }
inline float Sign(float v) { return v < 0.0f ? -1.0f : 1.0f; }

// Rotate a point around a pivot by angleDeg (matches Transform.RotateAround for a
// pure Z-axis rotation, which is all the 2D game ever does).
inline Vec2 RotateAround(const Vec2 &point, const Vec2 &pivot, float angleDeg)
{
    float rad = angleDeg * (CRUX_PI / 180.0f);
    float s = sinf(rad), c = cosf(rad);
    Vec2 d = point - pivot;
    return Vec2(pivot.x + d.x * c - d.y * s, pivot.y + d.x * s + d.y * c);
}

// Matches Unity's Mathf.SmoothDamp (the standard critically-damped-spring
// approximation) so CameraController.cs's SmoothDamp-based follow feels the same.
inline float SmoothDamp(float current, float target, float &velocity, float smoothTime, float maxSpeed, float dt)
{
    smoothTime = fmaxf(0.0001f, smoothTime);
    float omega = 2.0f / smoothTime;
    float x = omega * dt;
    float expo = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    float change = current - target;
    float originalTo = target;

    float maxChange = maxSpeed * smoothTime;
    change = Clamp(change, -maxChange, maxChange);
    target = current - change;

    float temp = (velocity + omega * change) * dt;
    velocity = (velocity - omega * temp) * expo;
    float output = target + (change + temp) * expo;

    if ((originalTo - current > 0.0f) == (output > originalTo))
    {
        output = originalTo;
        velocity = (output - originalTo) / dt;
    }
    return output;
}

inline Vec2 SmoothDamp(Vec2 current, Vec2 target, Vec2 &velocity, float smoothTime, float maxSpeed, float dt)
{
    return Vec2(
        SmoothDamp(current.x, target.x, velocity.x, smoothTime, maxSpeed, dt),
        SmoothDamp(current.y, target.y, velocity.y, smoothTime, maxSpeed, dt));
}

#endif
