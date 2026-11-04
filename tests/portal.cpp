#include "Transit.h"
#include <cstdlib>
#include <iostream>
void Check(bool v,const char* message){if(!v){std::cerr<<message<<'\n';std::exit(1);}}
int main(){
    using namespace Portal;
    Check(ActivePerspective(true,true)==1,"First person takes precedence when both body graphs render");
    Check(ActivePerspective(false,true)==0 && ActivePerspective(false,false)==-1,"One active view or none");
    Check(Transition(0,3)==Phase::Draw && Transition(3,0)==Phase::Sheathe,"OmenRim 0-3-0 trace");
    Transit t;t.Start(Phase::Draw);t.Mark(Phase::Draw);Check(t.markerAt<0,"Instant native attach is not visual handoff");
    for(int i=0;i<20;++i)t.Step(.016f);
    const float progress=t.DrawProgress(),time=t.time;t.Mark(Phase::Draw);
    Check(t.time==time && std::abs(t.DrawProgress()-progress)<.0001f,"Marker does not rewind time or pop proxy");
    t.Mark(Phase::Draw);Check(t.time==time,"Duplicate marker ignored");
    for(int i=0;i<11;++i)t.Step(.016f);
    Check(t.DrawProgress()>.999f,"Proxy converges exactly onto live grip");
    for(int i=0;i<100;++i){
        t.Start(Phase::Draw);t.Step(.10f);float open=t.Envelope();
        t.Start(Phase::Sheathe);Check(t.Envelope()>=open-.0001f,"Rapid reversal keeps portal open");
        t.Mark(Phase::Draw);Check(t.markerAt<0,"Stale draw marker cannot release sheath");
        for(int j=0;j<16;++j)t.Step(.10f);
        Check(t.phase==Phase::Idle && t.ReturnProgress()>.999f,"Missing native marker falls back and finishes absorption");
    }
    Check(SliceVisible(31.9f,32) && !SliceVisible(32.1f,32),"Far-side slices culled independently of camera/ground");
    t.Reset();t.Start(Phase::Draw,true);
    for(int i=0;i<20;++i)t.Step(.016f);
    t.Mark(Phase::Draw);Check(t.markerAt<0 && t.end==2.10f,"Early vanilla draw event cannot shorten first-person choreography");
    float before=t.DrawProgress();for(int i=0;i<25;++i)t.Step(.016f);
    Check(t.DrawProgress()>before && t.DrawProgress()<1,"First-person extraction spans visible motion frames");
    t.Start(Phase::Sheathe,true);Check(t.end==1.90f,"Separate authored return duration");
    for(int i=0;i<78;++i)t.Step(1.f/60);Check(t.ReturnProgress()<.01f,"Staff remains at animated grip before release");
    for(int i=0;i<12;++i)t.Step(1.f/60);Check(t.ReturnProgress()>.4f && t.ReturnProgress()<.7f,"Mid-return is visible, not a one-frame disappearance");
    for(int i=0;i<16;++i)t.Step(1.f/60);Check(t.phase==Phase::Sheathe && t.ReturnProgress()>.99f,"Tail clears before portal closure");
    t.Reset();Check(t.phase==Phase::Idle&&t.Envelope()==0,"Load/death interruption reset");
    std::cout<<"PASS transit05 single view, exact live-grip convergence, early/duplicate markers, 100 reversals, missing marker, slice plane\n";
}
