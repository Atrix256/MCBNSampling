#pragma once

#include <array>

static const float c_pi = 3.14159265359f;

float Clamp(float value, float themin, float themax)
{
    if (value <= themin)
        return themin;
    if (value >= themax)
        return themax;
    return value;
}

template <size_t N>
inline float Dot(const std::array<float, N>& A, const std::array<float, N>& B)
{
    float ret = 0.0f;
    for (size_t i = 0; i < N; ++i)
        ret += A[i] * B[i];
    return ret;
}

template <size_t N>
inline std::array<float, N> operator -(const std::array<float, N>& A, const std::array<float, N>& B)
{
    std::array<float, N> ret;
    for (size_t i = 0; i < N; ++i)
        ret[i] = A[i] - B[i];
    return ret;
}

template <size_t N>
inline float ToroidalDistanceSq(const std::array<float, N>& A, const std::array<float, N>& B, float domainSize = 1.0f)
{
    std::array<float, N> v = B - A;

    for (float& f : v)
    {
        f = std::abs(f);
        f = std::min(f, domainSize - f);
    }

    return Dot(v, v);
}

template <size_t N>
inline float ToroidalDistance(const std::array<float, N>& A, const std::array<float, N>& B, float domainSize = 1.0f)
{
    return std::sqrt(ToroidalDistanceSq(A, B, domainSize));
}

template <size_t N>
inline float DistanceSq(const std::array<float, N>& A, const std::array<float, N>& B)
{
    std::array<float, N> v = B - A;
    return Dot(v, v);
}

template <size_t N>
inline float Distance(const std::array<float, N>&A, const std::array<float, N>&B)
{
    return std::sqrt(DistanceSq(A, B));
}

float SmoothStep(float edge0, float edge1, float x)
{
    if (x < edge0)
        return 0;

    if (x >= edge1)
        return 1;

    // Scale/bias into [0..1] range
    x = (x - edge0) / (edge1 - edge0);

    return x * x * (3 - 2 * x);
}

inline float Lerp(float A, float B, float t)
{
    return A * (1.0f - t) + B * t;
}