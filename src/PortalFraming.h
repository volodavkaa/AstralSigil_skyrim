#pragma once
namespace Portal {
RE::NiCamera* FindCamera(RE::NiAVObject* object) {
    if(!object)return nullptr;
    if(auto* camera=netimmerse_cast<RE::NiCamera*>(object))return camera;
    if(auto* node=object->AsNode())for(auto& child:node->GetChildren())
        if(auto* camera=FindCamera(child.get()))return camera;
    return nullptr;
}
float FirstPersonFit(RE::NiNode* parent,RE::NiTransform& local) {
    constexpr float desired=.42f;
    auto* playerCamera=RE::PlayerCamera::GetSingleton();
    if(!playerCamera || !playerCamera->IsInFirstPerson())return desired;
    auto* camera=FindCamera(playerCamera->cameraRoot.get());if(!camera)return desired;
    auto project=[&](RE::NiPoint3 point,float& x,float& y) {
        float z=0;
        return RE::NiCamera::WorldPtToScreenPt3(camera->GetRuntimeData().worldToCam,
            camera->GetRuntimeData2().port,point,x,y,z,1.e-5f) && std::isfinite(x) && std::isfinite(y);
    };

    for(int iteration=0;iteration<3;++iteration) {
        auto center=parent->world*local.translate;
        float x,y,xx,xy,zx,zy;
        if(!project(center,x,y) || !project(parent->world*(local.translate+RE::NiPoint3{1,0,0}),xx,xy) ||
           !project(parent->world*(local.translate+RE::NiPoint3{0,0,1}),zx,zy))break;
        float a=xx-x,b=zx-x,c=xy-y,d=zy-y,det=a*d-b*c;
        if(std::abs(det)<1.e-7f)break;
        const float dx=.70f-x,dy=.50f-y;
        local.translate.x+=std::clamp((dx*d-b*dy)/det,-24.f,24.f);
        local.translate.z+=std::clamp((a*dy-dx*c)/det,-36.f,36.f);
    }
    float scale=desired;
    for(int attempt=0;attempt<12;++attempt) {
        local.scale=scale;const auto world=parent->world*local;bool inside=true;
        for(int i=0;i<32;++i) {
            const float angle=i*6.283185307f/32;float x,y;
            if(!project(world*RE::NiPoint3{32.f*std::cos(angle),-2,68.f*std::sin(angle)},x,y) ||
               x<.08f || x>.92f || y<.08f || y>.92f) {inside=false;break;}
        }
        if(inside)return scale;
        scale*=.88f;
    }
    return scale;
}
}
