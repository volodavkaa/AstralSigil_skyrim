#pragma once
#include <array>
#include <cstdint>

namespace Portal::Runtime {
using Version = std::array<std::uint16_t, 4>;

inline constexpr std::array<Version, 15> Candidates{{
    {1,5,97,0},
    {1,6,317,0}, {1,6,318,0}, {1,6,323,0}, {1,6,342,0}, {1,6,353,0},
    {1,6,629,0}, {1,6,640,0}, {1,6,659,1},
    {1,6,1130,0}, {1,6,1170,0}, {1,6,1170,1}, {1,6,1179,1},
    {1,7,99,0}, {1,7,104,0}
}};
constexpr bool Accepts(Version version) {
    for (auto candidate : Candidates) if (candidate == version) return true;
    return false;
}
}
