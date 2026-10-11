#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace c286 {
inline constexpr float lineGainMaximum=45.f;
inline constexpr float lineGainMinimum(bool modern)noexcept{return modern?-15.f:-5.f;}
inline float effectiveLineGain(float requested,bool modern)noexcept{
 return std::isfinite(requested)?std::clamp(requested,lineGainMinimum(modern),lineGainMaximum):0.f;
}
inline constexpr std::array<double,5> lineGainMarks(bool modern)noexcept{
 return modern?std::array<double,5>{{-15,0,15,30,45}}:std::array<double,5>{{-5,0,15,25,45}};
}
}
