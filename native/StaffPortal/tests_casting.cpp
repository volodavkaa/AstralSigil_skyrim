#include "CastingAnchor.h"
#include <cstdlib>
#include <iostream>
#include <limits>

void Check(bool v,const char* message){if(!v){std::cerr<<message<<'\n';std::exit(1);}}
float Error(RE::NiPoint3 a,RE::NiPoint3 b){auto d=a-b;return std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);}

int main(){
    for(int state : {1,2,3,4,5,6,7})Check(Casting::ActiveNativeState(state),"Precharge, charge, ready and release preparation all require the same attachment");
    for(int state : {-1,0,8,9,10})Check(!Casting::ActiveNativeState(state),"Idle/interruption/invalid states must not extend the active window");
    RE::NiTransform offset,parent,staff;offset.translate={0,14,0};
    staff.translate={20,30,100};
    auto pose=Casting::AnchorPose(parent,staff,offset);auto p=pose.translate;
    Check(Error(p,{20,102.40428829f,100})<.0001f,"Identity frame uses actual tip plus authored clearance");
    // Aim/hand and weapon use different orientations, scales and translations.
    // Simulate continuous movement and ready-loop pose changes, not one offset.
    for(int frame=0;frame<480;++frame){
        float a=frame*.037f,b=frame*.019f,c=std::cos(a),s=std::sin(a);
        parent.rotate.entry[0][0]=c;parent.rotate.entry[0][1]=-s;parent.rotate.entry[0][2]=0;
        parent.rotate.entry[1][0]=s;parent.rotate.entry[1][1]=c;parent.rotate.entry[1][2]=0;
        parent.rotate.entry[2][0]=0;parent.rotate.entry[2][1]=0;parent.rotate.entry[2][2]=1;
        parent.translate={100+frame*.8f,-50+10*std::cos(b),70+12*std::sin(b)};parent.scale=.65f+.8f*(1+std::sin(b))*.5f;
        staff.rotate.entry[0][0]=1;staff.rotate.entry[0][1]=0;staff.rotate.entry[0][2]=0;
        staff.rotate.entry[1][0]=0;staff.rotate.entry[1][1]=std::cos(b);staff.rotate.entry[1][2]=-std::sin(b);
        staff.rotate.entry[2][0]=0;staff.rotate.entry[2][1]=std::sin(b);staff.rotate.entry[2][2]=std::cos(b);
        staff.translate={125+frame*.8f,-35+7*std::cos(b),110+6*std::sin(b)};staff.scale=1.2f;
        pose=Casting::AnchorPose(parent,staff,offset);p=pose.translate;
        auto expected=staff*RE::NiPoint3{0,Casting::StaffTip+14,0};
        Check(Error(parent*p,expected)<.0003f,"Walking/aim transforms must keep anchor beyond the current staff tip");
        auto world=parent*pose;
        for(auto point : {RE::NiPoint3{0,20,0},RE::NiPoint3{15,0,8},RE::NiPoint3{-4,7,-13}}){
            auto target=staff*(point+RE::NiPoint3{0,Casting::StaffTip+14,0});
            Check(Error(world*point,target)<.0004f,"Entire casting frame must follow shaft, independent of native ready rotation");
        }
        Check(Casting::Valid(parent)&&Casting::Valid(staff)&&Casting::Finite(p),"Valid moving transform accepted");
    }
    parent.scale=0;Check(!Casting::Valid(parent),"Zero parent scale rejected before inversion");
    parent.scale=1;parent.translate.x=std::numeric_limits<float>::quiet_NaN();Check(!Casting::Valid(parent),"Non-finite parent rejected");
    std::cout<<"PASS casting frame: complete active-state window including 2/4, tip, shaft direction, 3 spatial probes over 480 ready/moving poses, scale, invalid transforms\n";
}
