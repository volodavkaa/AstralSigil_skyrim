#pragma once
#include <algorithm>

namespace Portal {
inline constexpr float drawSeconds=1.35f,sheatheSeconds=1.30f;
enum class Phase { Idle, Draw, Sheathe };

inline Phase Transition(int previous,int current) {
    if(current==2 || (current==1 && previous==0) || (current==3 && previous==0))return Phase::Draw;
    if(current==5 || (current==4 && previous==3) || (current==0 && previous==3))return Phase::Sheathe;
    return Phase::Idle;
}
}
