#pragma once
#include <cstddef>
#include <cmath>
#include <RE/N/NiTransform.h>

namespace Casting {
inline constexpr float StaffTip = 58.40428829f;

inline constexpr bool ActiveNativeState(int state) {
    return state>=1 && state<=7;
}

inline bool Finite(const RE::NiPoint3& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
inline bool Valid(const RE::NiTransform& t) {
    if (!Finite(t.translate) || !std::isfinite(t.scale) || t.scale <= .001f) return false;
    for (const auto& row : t.rotate.entry) for (float value : row) if (!std::isfinite(value)) return false;
    return true;
}

inline RE::NiTransform AnchorPose(const RE::NiTransform& parent,
                                 const RE::NiTransform& staff,
                                 RE::NiTransform authoredPose) {
    authoredPose.translate += RE::NiPoint3{0, StaffTip, 0};
    return parent.Invert() * staff * authoredPose;
}
}
