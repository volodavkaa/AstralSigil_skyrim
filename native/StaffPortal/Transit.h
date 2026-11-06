#pragma once
#include "PortalPhase.h"
#include <cmath>
namespace Portal {
inline float Smooth(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
struct Transit {
    Phase phase=Phase::Idle;
    float time=0,markerAt=-1,end=0,initialOpen=0;
    bool nativeMarker=false,firstPerson=false;
    void Reset(){*this={};}
    void Start(Phase next,bool first=false){float open=Envelope();*this={};phase=next;firstPerson=first;initialOpen=open;end=first?(next==Phase::Draw?2.10f:1.90f):(next==Phase::Draw?drawSeconds:sheatheSeconds);}
    void Mark(Phase expected,bool native=true){
        if(phase!=expected || markerAt>=0 || (native && time<.08f))return;
        if(native && firstPerson && time<(phase==Phase::Draw?.85f:1.10f))return;
        markerAt=time;nativeMarker=native;
        if(firstPerson)return;
        if(phase==Phase::Draw)end=std::max(time+.34f,time+(drawSeconds-.65f)*std::clamp(time/.65f,.45f,1.5f));
        else end=std::max(sheatheSeconds,time+.40f);
    }
    float DrawProgress()const{
        if(firstPerson)return Smooth((time-.30f)/.75f);
        if(markerAt<0)return .65f*Smooth(time/.65f);
        float start=.65f*Smooth(markerAt/.65f);
        return start+(1-start)*Smooth((time-markerAt)/.16f);
    }
    float ReturnProgress()const{
        if(firstPerson)return Smooth((time-1.30f)/.40f);
        return markerAt<0?0:Smooth((time-markerAt)/.30f);
    }
    float Envelope()const{
        if(phase==Phase::Idle)return 0;
        return std::min(std::max(initialOpen,Smooth((time-.02f)/.18f)),Smooth((end-time)/.16f));
    }
    void Step(float dt){
        if(phase==Phase::Idle)return;
        time+=std::clamp(dt,0.f,.10f);
        float grip=firstPerson?(phase==Phase::Draw?1.05f:1.30f):(phase==Phase::Draw?.65f:.84f);
        if(markerAt<0 && time>=grip)Mark(phase,false);
        if(time>=end)phase=Phase::Idle;
    }
};
inline bool SliceVisible(float localY,float planeY){return localY<=planeY;}
inline int ActivePerspective(bool first,bool third){return first?1:third?0:-1;}
}
