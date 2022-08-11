#pragma once

#include <array>

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
inline float ToroidalDistanceSq(const std::array<float, N>& A, const std::array<float, N>& B)
{
    std::array<float, N> v = B - A;

    for (float& f : v)
        f = std::min(f, 1.0f - f);

    return Dot(v, v);
}

template <size_t N>
inline float ToroidalDistance(const std::array<float, N>& A, const std::array<float, N>& B)
{
    return std::sqrt(ToroidalDistanceSq(A, B));
}
