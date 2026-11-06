#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <cmath>
#include "Transit.h"
#include "PortalFraming.h"
#include "CastingAnchor.h"
#ifdef ASTRAL_SIGIL_MULTI_RUNTIME
#include "RuntimePolicy.h"
#endif

namespace {
using Clock=std::chrono::steady_clock;
RE::ActorState* ActorState(RE::PlayerCharacter* player){
#ifdef ASTRAL_SIGIL_MULTI_RUNTIME
    return player->AsActorState();
#else
    return static_cast<RE::ActorState*>(player);
#endif
}
std::atomic_bool armed=false,pending=false;
std::atomic_uint markers=0;
std::array<RE::TESObjectWEAP*,2> weapons{}, maces{};
RE::TESGlobal* gripTransition=nullptr;
bool wasGripTransition=false;
Portal::Transit transit;
int variant=-1,lastWeaponState=-1,lastView=-1;
RE::NiAVObject* registeredRoot=nullptr;
Clock::time_point previousTick=Clock::now();

struct Hidden {RE::NiPointer<RE::NiAVObject> object;bool baseline,seen;};
std::vector<Hidden> hidden;
void Restore(){for(auto& h:hidden)h.object->SetAppCulled(h.baseline);hidden.clear();}
void Hide(RE::NiAVObject* object){
    auto it=std::find_if(hidden.begin(),hidden.end(),[&](const auto& h){return h.object.get()==object;});
    if(it==hidden.end())hidden.push_back({RE::NiPointer<RE::NiAVObject>(object),object->GetAppCulled(),true});
    else it->seen=true;
    object->SetAppCulled(true);
}
std::vector<RE::BSSoundHandle> sounds;
bool closePlayed=false;
void StopSounds(){for(auto& s:sounds)if(s.IsValid())s.FadeOutAndRelease(120);sounds.clear();closePlayed=false;}
void Sound(RE::FormID id,float volume){
    auto* audio=RE::BSAudioManager::GetSingleton();auto* d=RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(id);
    if(!audio || !d)return;
    RE::BSSoundHandle s;
#ifdef ASTRAL_SIGIL_MULTI_RUNTIME
    const bool built=audio->GetSoundHandle(s,d);
#else
    const bool built=audio->BuildSoundDataFromDescriptor(s,d);
#endif
    if(built){s.SetVolume(volume);s.Play();sounds.push_back(s);}
}
RE::NiPointer<RE::NiAVObject> Load(const char* path,const char* name){
    RE::NiPointer<RE::NiNode> source;
    auto result=RE::BSModelDB::Demand(path,source,RE::BSModelDB::DBTraits::ArgsType{});
    if(result!=RE::BSResource::ErrorCode::kNone || !source){SKSE::log::error("Cannot load {} code={}",path,static_cast<int>(result));return {};}
#ifdef ASTRAL_SIGIL_MULTI_RUNTIME
    RE::NiPointer<RE::NiObject> cloned(source->Clone());
    RE::NiPointer<RE::NiAVObject> copy(netimmerse_cast<RE::NiAVObject*>(cloned.get()));
#else
    RE::NiPointer<RE::NiAVObject> copy(source->Clone());
#endif
    if(copy)copy->name=name;return copy;
}
struct Scene {
    RE::NiPointer<RE::NiNode> parent;
    RE::NiPointer<RE::NiAVObject> portal,proxy;
    RE::NiAVObject* whole=nullptr;RE::NiAVObject* sliced=nullptr;
    std::vector<RE::NiAVObject*> slices;
    RE::NiTransform seed,released;
    bool seeded=false,hasRelease=false;
    int view=-1,variant=-1;
    void Clear(){
        whole=sliced=nullptr;slices.clear();
        for(auto* o:{portal.get(),proxy.get()})if(o && o->parent)o->parent->DetachChild(o);
        portal.reset();proxy.reset();parent.reset();view=variant=-1;seeded=hasRelease=false;
    }
} scene;
struct HandCache {RE::NiTransform offset,lastPose;bool valid=false,hasPose=false;};
std::array<HandCache,2> handCache;
std::array<std::array<float,2>,2> staffSizes{};

struct CastAnchor {
    RE::NiPointer<RE::NiAVObject> node;
    RE::NiTransform baseline;
    int lastState=-1;
    bool reportedMissing=false;
    Clock::time_point lastActive{};
    void Clear(){

        if(node)node->local=baseline;
        node.reset();lastState=-1;reportedMissing=false;lastActive={};
    }
} castAnchor;

void RefreshCastTransforms(RE::NiAVObject* object,RE::NiUpdateData& update){
    object->UpdateWorldData(&update);
    if(auto* node=object->AsNode())for(auto& child:node->GetChildren())if(child)RefreshCastTransforms(child.get(),update);
    object->UpdateWorldBound();
}

RE::NiAVObject* HeldModel(RE::NiAVObject* object,bool right);
void AnchorCast(RE::PlayerCharacter* player,RE::NiAVObject* third,int view,bool drawn){
    auto* caster=player->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand);
    if(view!=0 || !drawn || !third || variant<0 || !caster || caster->currentSpell!=weapons[variant]->formEnchanting || transit.phase!=Portal::Phase::Idle){
        castAnchor.Clear();return;
    }
    bool casting=Casting::ActiveNativeState(static_cast<int>(caster->state.get()));
    auto now=Clock::now();

    if(!casting && (!castAnchor.node || std::chrono::duration<float>(now-castAnchor.lastActive).count()>.55f)){
        castAnchor.Clear();return;
    }

    auto* anchor=third->GetObjectByName(RE::BSFixedString("AS_Offset"));
    if(!anchor || !anchor->parent){
        bool reported=castAnchor.reportedMissing;castAnchor.Clear();castAnchor.reportedMissing=true;
        if(!reported)SKSE::log::warn("Casting anchor: own third-person AS_Offset not attached yet, state={}",static_cast<int>(caster->state.get()));
        return;
    }
    auto* staff=(scene.view==0 && scene.variant==variant && scene.whole && scene.proxy && !scene.proxy->GetAppCulled())?scene.whole:HeldModel(third,false);
    if(!staff || !Casting::Valid(staff->world) || !Casting::Valid(anchor->parent->world)){
        if(!castAnchor.reportedMissing){SKSE::log::warn("Casting anchor waiting for valid third-person staff/parent transform");castAnchor.reportedMissing=true;}
        return;
    }
    if(castAnchor.node.get()!=anchor){
        castAnchor.Clear();castAnchor.node.reset(anchor);castAnchor.baseline=anchor->local;
        SKSE::log::info("Casting anchor acquired third-person art, clearance=({:.2f},{:.2f},{:.2f})",castAnchor.baseline.translate.x,castAnchor.baseline.translate.y,castAnchor.baseline.translate.z);
    }
    if(casting)castAnchor.lastActive=now;
    auto pose=Casting::AnchorPose(anchor->parent->world,staff->world,castAnchor.baseline);
    if(!Casting::Valid(pose))return;
    anchor->local=pose;
    RE::NiUpdateData update{};update.flags=RE::NiUpdateData::Flag::kDirty;
    RefreshCastTransforms(anchor,update);
    int state=static_cast<int>(caster->state.get());
    if(state!=castAnchor.lastState){
        auto tip=staff->world*RE::NiPoint3{0,Casting::StaffTip,0};
        SKSE::log::info("Casting anchor state={} tip=({:.1f},{:.1f},{:.1f})",state,tip.x,tip.y,tip.z);castAnchor.lastState=state;
    }
}

bool OwnName(const char* name){
    if(!name)return false;
    std::string_view n=name;return n.starts_with("AS_Dragon_golden_")||n.starts_with("AS_Dragon_silver_")||n.starts_with("AS_Melee_golden_")||n.starts_with("AS_Melee_silver_");
}
bool VariantName(const char* name){
    if(!name || variant<0)return false;
    const std::string_view n=name;
    return n.starts_with(variant==0?"AS_Dragon_golden_":"AS_Dragon_silver_")||n.starts_with(variant==0?"AS_Melee_golden_":"AS_Melee_silver_");
}
RE::NiAVObject* HeldModel(RE::NiAVObject* object,bool right=false){
    if(!object)return nullptr;
    if(object->name=="AS_PortalRoot" || object->name=="AS_PortalProxy")return nullptr;
    if(object->name=="Weapon")right=true;
    if(right && VariantName(object->name.c_str()))return object->parent;
    if(auto* node=object->AsNode())for(auto& child:node->GetChildren())if(auto* found=HeldModel(child.get(),right))return found;
    return nullptr;
}
void Scan(RE::NiAVObject* object,bool right,bool left,bool rightAllowed){
    if(!object)return;
    if(object->name=="AS_PortalRoot" || object->name=="AS_PortalProxy"){

        if(object!=scene.portal.get() && object!=scene.proxy.get())object->SetAppCulled(true);
        return;
    }
    if(object->name=="Weapon")right=true;
    if(object->name=="Shield" || object->name=="WeaponLeft")left=true;
    if(OwnName(object->name.c_str()) && !left && (!right || !rightAllowed || !VariantName(object->name.c_str())))Hide(object);
    if(auto* node=object->AsNode())for(auto& child:node->GetChildren())Scan(child.get(),right,left,rightAllowed);
}
void Visibility(RE::NiAVObject* third,RE::NiAVObject* first,bool controlled,bool drawn){
    for(auto& h:hidden)h.seen=false;
    Scan(third,false,false,drawn&&!controlled);if(first!=third)Scan(first,false,false,drawn&&!controlled);
    std::erase_if(hidden,[](auto& h){if(h.seen)return false;h.object->SetAppCulled(h.baseline);return true;});
}
void Reset(){castAnchor.Clear();scene.Clear();Restore();StopSounds();transit.Reset();handCache={};lastView=-1;}
void MatchStaffScale(RE::NiAVObject* object,int view,bool mace,bool drawn){
    auto* root=object?object->AsNode():nullptr;if(!root||!drawn||variant<0)return;
    auto* model=HeldModel(root);auto* weapon=root->GetObjectByName(RE::BSFixedString("Weapon"));
    if(!model||!weapon||!Casting::Valid(model->world)||!Casting::Valid(weapon->world))return;
    auto& size=staffSizes[variant][view];
    if(!mace)size=model->world.scale/root->world.scale;
    else if(size>.05f){
        float ratio=size*root->world.scale/model->world.scale;
        if(std::isfinite(ratio)&&std::abs(ratio-1.f)>.0001f){
            model->local.scale*=ratio;
            RE::NiUpdateData update{};update.flags=RE::NiUpdateData::Flag::kDirty;RefreshCastTransforms(model,update);
        }
    }
}

void Start(Portal::Phase phase,int view){
    SKSE::log::info("Portal transit start phase={} perspective={}",static_cast<int>(phase),view);
    bool reversal=transit.phase!=Portal::Phase::Idle;
    if(scene.proxy){scene.seed=scene.proxy->local;scene.seeded=reversal;scene.hasRelease=false;}
    transit.Start(phase,view==1);StopSounds();
    Sound(0x86efd,.65f);if(variant==1)Sound(0x3f2b1,.35f);
}
bool Prepare(RE::NiNode* root,int view){
    if(scene.parent.get()==root && scene.variant==variant && scene.view==view && scene.proxy && scene.portal)return true;

    scene.Clear();scene.parent.reset(root);scene.view=view;scene.variant=variant;
    scene.portal=Load(variant==0?"meshes\\astralsigil\\portal\\frost.nif":"meshes\\astralsigil\\portal\\fire.nif","AS_PortalRoot");
    scene.proxy=Load(variant==0?"meshes\\astralsigil\\portal\\golden_sliced.nif":"meshes\\astralsigil\\portal\\silver_sliced.nif","AS_PortalProxy");
    if(!scene.portal || !scene.proxy){scene.Clear();return false;}
    scene.whole=scene.proxy->GetObjectByName(RE::BSFixedString("AS_Whole"));
    scene.sliced=scene.proxy->GetObjectByName(RE::BSFixedString("AS_Sliced"));
    for(int i=0;i<40;++i){auto name=std::format("AS_Slice{:02}",i);auto* s=scene.proxy->GetObjectByName(RE::BSFixedString(name));if(s)scene.slices.push_back(s);}
    if(!scene.whole || !scene.sliced || scene.slices.size()!=40){SKSE::log::error("Incomplete sliced proxy");scene.Clear();return false;}
    scene.portal->SetAppCulled(true);scene.proxy->SetAppCulled(true);
    root->AttachChild(scene.portal.get());root->AttachChild(scene.proxy.get());
    SKSE::log::info("Single scene view={} variant={} slices={}",view,variant,scene.slices.size());return true;
}
RE::NiTransform LivePose(RE::NiNode* root,int view,bool drawn){
    auto* weapon=root->GetObjectByName(RE::BSFixedString("Weapon"));
    auto* model=drawn?HeldModel(root):nullptr;
    auto inverse=root->world.Invert();
    if(model && weapon){handCache[view].offset=weapon->world.Invert()*model->world;handCache[view].valid=true;}
    if(weapon){
        auto pose=inverse*weapon->world*(handCache[view].valid?handCache[view].offset:RE::NiTransform{});
        if(std::isfinite(pose.scale) && pose.scale>.05f){handCache[view].lastPose=pose;handCache[view].hasPose=true;return pose;}
        if(handCache[view].hasPose)return handCache[view].lastPose;
    }
    if(model)return inverse*model->world;
    RE::NiTransform fallback;fallback.translate={24,22,90};return fallback;
}
bool Render(RE::NiAVObject* object,int view,bool drawn){
    auto* root=object?object->AsNode():nullptr;if(!root || !Prepare(root,view))return false;
    auto pose=LivePose(root,view,drawn);
    if(auto* player=RE::PlayerCharacter::GetSingleton();player&&player->GetEquippedObject(false)==maces[variant])
        pose.translate+=pose.rotate*RE::NiPoint3{0,12*pose.scale,0};
    bool active=transit.phase!=Portal::Phase::Idle;
    scene.portal->local.translate=view==1?RE::NiPoint3{17,62,120}:RE::NiPoint3{18,32,98};
    if(view==1 && active){

        if(transit.phase==Portal::Phase::Draw)scene.portal->local.translate.y+=40*Portal::Smooth((transit.time-1.10f)/.65f);
        else scene.portal->local.translate.y+=58*(1-Portal::Smooth((transit.time-.70f)/.60f));
    }
    float portalScale=view==1?Portal::FirstPersonFit(root,scene.portal->local):1.f;
    scene.portal->local.scale=std::max(.001f,portalScale*transit.Envelope());
    scene.portal->SetAppCulled(!active);
    if(active){

        auto distant=scene.portal->local.translate+RE::NiPoint3{0,140,0}-pose.rotate*RE::NiPoint3{0,8*pose.scale,0};
        auto lip=[&](const RE::NiTransform& p){
            float axis=p.rotate.entry[1][1]*p.scale;
            float tip=axis>=0?-42.09078f:58.40429f;
            auto at=scene.portal->local.translate-p.rotate*RE::NiPoint3{0,tip*p.scale,0};
            at.y+=3.f;
            return at;
        };
        if(view==1)distant=lip(pose);
        if(transit.phase==Portal::Phase::Draw){
            float u=transit.DrawProgress();pose.translate=distant*(1-u)+pose.translate*u;
        }else if(transit.markerAt>=0){
            if(!scene.hasRelease){scene.released=pose;scene.hasRelease=true;}
            pose=scene.released;
            distant=scene.portal->local.translate+RE::NiPoint3{0,140,0}-pose.rotate*RE::NiPoint3{0,8*pose.scale,0};
            if(view==1)distant=lip(pose);
            float u=transit.ReturnProgress();pose.translate=scene.released.translate*(1-u)+distant*u;
        }
        if(scene.seeded && transit.time<.12f){float u=Portal::Smooth(transit.time/.12f);pose.translate=scene.seed.translate*(1-u)+pose.translate*u;}
    }

    scene.proxy->local=pose;scene.proxy->SetAppCulled(!active);
    scene.whole->SetAppCulled(active);scene.sliced->SetAppCulled(!active);
    if(active)for(auto* slice:scene.slices){auto p=pose*slice->local.translate;slice->SetAppCulled(!Portal::SliceVisible(p.y,scene.portal->local.translate.y));}
    RE::NiUpdateData update{};update.time=transit.time;update.flags=RE::NiUpdateData::Flag::kDirty;
    scene.portal->Update(update);scene.proxy->Update(update);
    return active;
}
class Sink final:public RE::BSTEventSink<RE::BSAnimationGraphEvent>{
    RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* e,RE::BSTEventSource<RE::BSAnimationGraphEvent>*)override{
        if(e && e->holder==RE::PlayerCharacter::GetSingleton()){
            if(e->tag=="weaponDraw")markers.fetch_or(1);else if(e->tag=="weaponSheathe")markers.fetch_or(2);
        }return RE::BSEventNotifyControl::kContinue;
    }
}sink;
void Tick(){
    pending=false;auto now=Clock::now();float dt=std::chrono::duration<float>(now-previousTick).count();previousTick=now;
    if(!armed)return;
    auto* player=RE::PlayerCharacter::GetSingleton();auto* ui=RE::UI::GetSingleton();
    if(!player || !ui || !player->Is3DLoaded() || player->IsDead()){Reset();lastWeaponState=-1;registeredRoot=nullptr;markers=0;return;}
    if(ui->GameIsPaused())return;
    auto* third=player->Get3D(false);auto* first=player->Get3D(true);
    if(third!=registeredRoot){Reset();registeredRoot=third;lastWeaponState=-1;markers=0;}
    player->AddAnimationGraphEventSink(&sink);
    if(player->IsInRagdollState() || player->IsInKillMove() || ActorState(player)->IsSwimming() || ActorState(player)->IsBleedingOut()){
        Reset();lastWeaponState=-1;Visibility(third,first,false,ActorState(player)->IsWeaponDrawn());return;
    }
    auto* right=player->GetEquippedObject(false);auto* left=player->GetEquippedObject(true);
    int current=(right==weapons[0]||right==maces[0])?0:(right==weapons[1]||right==maces[1])?1:-1;
    if(gripTransition&&gripTransition->value>.5f){
        if(!wasGripTransition){Reset();wasGripTransition=true;}
        variant=current;
        bool held=ActorState(player)->IsWeaponDrawn();
        if(current>=0){MatchStaffScale(third,0,right==maces[current],held);MatchStaffScale(first,1,right==maces[current],held);}
        Visibility(third,first,false,held);
        lastWeaponState=static_cast<int>(ActorState(player)->GetWeaponState());markers=0;return;
    }
    if(wasGripTransition){wasGripTransition=false;Reset();variant=current;lastWeaponState=static_cast<int>(ActorState(player)->GetWeaponState());markers=0;}
    auto* leftWeapon=left?left->As<RE::TESObjectWEAP>():nullptr;
    bool leftStaff=leftWeapon && leftWeapon->GetWeaponType()==RE::WEAPON_TYPE::kStaff;
    if(current<0 || leftStaff){Reset();variant=current;lastWeaponState=-1;Visibility(third,first,false,ActorState(player)->IsWeaponDrawn());markers=0;return;}
    if(current!=variant){Reset();variant=current;lastWeaponState=-1;markers=0;}
    bool held=ActorState(player)->GetWeaponState()==RE::WEAPON_STATE::kDrawn;
    MatchStaffScale(third,0,right==maces[current],held);MatchStaffScale(first,1,right==maces[current],held);
    auto* camera=RE::PlayerCamera::GetSingleton();
    int view=camera?Portal::ActivePerspective(camera->IsInFirstPerson(),camera->IsInThirdPerson()):-1;
    if(view<0){castAnchor.Clear();scene.Clear();Visibility(third,first,false,ActorState(player)->IsWeaponDrawn());return;}
    if(view!=lastView){scene.Clear();lastView=view;SKSE::log::info("Active perspective {}",view);}
    int ws=static_cast<int>(ActorState(player)->GetWeaponState());
    if(ws!=lastWeaponState){
        SKSE::log::info("Portal weapon state {} -> {}, form={:08X}",lastWeaponState,ws,right?right->GetFormID():0);
        auto next=Portal::Transition(lastWeaponState,ws);
        if(next!=Portal::Phase::Idle && next!=transit.phase){
            Start(next,view);
            if(next==Portal::Phase::Sheathe&&ws==0)transit.Mark(next,false);
        }
        lastWeaponState=ws;
    }
    unsigned events=markers.exchange(0);
    if(events&1){if(transit.firstPerson)SKSE::log::info("First-person draw marker at {:.3f}",transit.time);transit.Mark(Portal::Phase::Draw);}
    if(events&2){if(transit.firstPerson)SKSE::log::info("First-person return marker at {:.3f}",transit.time);transit.Mark(Portal::Phase::Sheathe);}
    if(transit.phase==Portal::Phase::Draw){
        auto* caster=player->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand);
        if(caster && caster->currentSpell==weapons[variant]->formEnchanting &&
           Casting::ActiveNativeState(static_cast<int>(caster->state.get()))){
            transit.Reset();StopSounds();SKSE::log::info("Draw presentation interrupted by actual staff casting");
        }
    }
    auto old=transit.phase;transit.Step(dt);
    bool drawn=ws==3 || ws==4 || ws==5;
    bool renderNeeded=transit.phase!=Portal::Phase::Idle || drawn;
    bool controlled=false;
    if(renderNeeded)controlled=Render(view==1?first:third,view,drawn);
    else scene.Clear();
    Visibility(third,first,controlled,drawn);
    AnchorCast(player,third,view,drawn);
    if(transit.phase!=Portal::Phase::Idle && !closePlayed && transit.end-transit.time<.18f){Sound(0x86efe,.6f);closePlayed=true;}
    if(old!=Portal::Phase::Idle && transit.phase==Portal::Phase::Idle)SKSE::log::info("Transit finished: nativeMarker={} held={} first={} seconds={:.3f}",transit.nativeMarker,drawn,transit.firstPerson,transit.time);
}
void Message(SKSE::MessagingInterface::Message* m){
    using M=SKSE::MessagingInterface;
    if(m->type==M::kDataLoaded){
        auto* d=RE::TESDataHandler::GetSingleton();weapons[0]=d->LookupForm<RE::TESObjectWEAP>(0x800,"AstralSigil.esp");weapons[1]=d->LookupForm<RE::TESObjectWEAP>(0x820,"AstralSigil.esp");
        maces[0]=d->LookupForm<RE::TESObjectWEAP>(0x940,"AstralSigil.esp");maces[1]=d->LookupForm<RE::TESObjectWEAP>(0x942,"AstralSigil.esp");
        gripTransition=d->LookupForm<RE::TESGlobal>(0x945,"AstralSigil.esp");
        SKSE::log::info("Mace portal forms ready={},{}",maces[0]!=nullptr,maces[1]!=nullptr);
        armed=weapons[0]&&weapons[1]&&REX::W32::GetModuleHandleW(L"OpenAnimationReplacer.dll");
        SKSE::log::info("Handoff05 enabled={}",armed.load());
        if(armed)std::thread([]{for(;;){std::this_thread::sleep_for(std::chrono::milliseconds(16));if(armed&&!pending.exchange(true))SKSE::GetTaskInterface()->AddTask(Tick);}}).detach();
    }else if(m->type==M::kPreLoadGame || m->type==M::kNewGame || m->type==M::kPostLoadGame){Reset();staffSizes={};wasGripTransition=false;markers=0;variant=lastWeaponState=-1;registeredRoot=nullptr;previousTick=Clock::now();}
}
}
#ifdef ASTRAL_SIGIL_MULTI_RUNTIME
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData data;
    data.PluginVersion(REL::Version(2,4,2,0));
    data.PluginName("AstralSigilPortal");
    data.UsesAddressLibrary();
    data.UsesUpdatedStructs();
    for (std::size_t i=0;i<Portal::Runtime::Candidates.size();++i) {
        const auto& v=Portal::Runtime::Candidates[i];
        data.compatibleVersions[i]=REL::Version(v[0],v[1],v[2],v[3]).pack();
    }
    return data;
}();
#endif
extern "C" __declspec(dllexport) bool SKSEPlugin_Query(const SKSE::QueryInterface* skse,SKSE::PluginInfo* info){
    info->infoVersion=SKSE::PluginInfo::kVersion;info->name="AstralSigilPortal";info->version=20402;
#ifdef ASTRAL_SIGIL_MULTI_RUNTIME
    info->version=20402;
    const auto v=skse->RuntimeVersion();
    return !skse->IsEditor()&&Portal::Runtime::Accepts({v[0],v[1],v[2],v[3]});
#else
    return !skse->IsEditor()&&skse->RuntimeVersion()==REL::Version(1,5,97,0);
#endif
}
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse){
#ifdef ASTRAL_SIGIL_MULTI_RUNTIME
    const auto v=skse->RuntimeVersion();
    if(skse->IsEditor() || !Portal::Runtime::Accepts({v[0],v[1],v[2],v[3]}))return false;
#endif
    SKSE::Init(skse);
    if(auto dir=SKSE::log::log_directory()){
        auto fileSink=std::make_shared<spdlog::sinks::basic_file_sink_mt>((*dir/"AstralSigilPortal.log").string(),true);
        spdlog::set_default_logger(std::make_shared<spdlog::logger>("AstralSigilPortal",fileSink));spdlog::flush_on(spdlog::level::info);
    }
#ifdef ASTRAL_SIGIL_MULTI_RUNTIME
    SKSE::log::info("Portal 2.4.2: runtime {}.{}.{}.{}; SE/AE relocations and dynamic layouts; runtime validation policy applies",v[0],v[1],v[2],v[3]);
#else
    SKSE::log::info("Portal 2.4.2: native equipped model after portal transit; casting frame correction retained, SE 1.5.97");
#endif
    return SKSE::GetMessagingInterface()->RegisterListener(Message);
}
