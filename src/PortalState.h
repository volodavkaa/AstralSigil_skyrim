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
struct State {
    Phase phase=Phase::Idle;
    float time=0;
    bool handoff=false;
    bool timedOut=false;
    void Reset() { *this=State{}; }
    void Start(Phase value) { Reset();phase=value; }
    void Marker(Phase value) {
        if (phase!=value || handoff) return;
        handoff=true;time=value==Phase::Draw?.65f:.84f;
    }
    void Step(float dt) {
        if (phase==Phase::Idle) return;
        time+=std::clamp(dt,0.f,.10f);
        if (time>(phase==Phase::Draw?drawSeconds:sheatheSeconds)) {
            timedOut=!handoff;phase=Phase::Idle;
        }
    }
    float Envelope() const {
        if (phase==Phase::Idle) return 0;
        const float end=phase==Phase::Draw?drawSeconds:sheatheSeconds;
        const float open=std::clamp((time-.06f)/.25f,0.f,1.f);
        const float close=std::clamp((end-time)/(phase==Phase::Draw?.18f:.12f),0.f,1.f);
        const float v=std::min(open,close);
        return v*v*(3-2*v);
    }
    bool Proxy() const {
        return (phase==Phase::Draw && !handoff && time>=.12f) ||
               (phase==Phase::Sheathe && handoff);
    }
    bool HandVisible(bool drawn) const {
        if (phase==Phase::Draw) return handoff;
        if (phase==Phase::Sheathe) return !handoff;
        return drawn;
    }
};
}
