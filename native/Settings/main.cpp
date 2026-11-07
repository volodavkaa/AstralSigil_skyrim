#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include "Schema.h"
#include "Transformation.h"
#ifdef ASTRAL_CLIP_TRACE
#include "ClipTrace.h"
#endif
#include "../StaffPortal/RuntimePolicy.h"

namespace {
using Clock=std::chrono::steady_clock;
using namespace Settings;
std::array<float,Count> values=Defaults;
std::array<RE::TESGlobal*,Count> globals{};
std::array<RE::BGSProjectile*,2> bolts{};
std::array<RE::BGSHazard*,2> hazardTypes{};
RE::SpellItem* fireSpell=nullptr;
std::array<RE::BGSExplosion*,2> explosionTypes{};
RE::BGSSoundDescriptorForm* iceSound=nullptr;
std::array<RE::TESObjectWEAP*,2> weapons{};
std::vector<RE::ObjectRefHandle> projectiles,hazards,effects;
struct HazardVisual {RE::NiAVObject* root=nullptr;float lifetime=0;std::uint64_t seen=0;};
std::unordered_map<RE::FormID,HazardVisual> hazardVisuals;
std::atomic_bool pending=false,enabled=false;
Clock::time_point castStart{},releaseTime{},cancelTime{},lastCombat{};
Clock::time_point lastTick=Clock::now();
bool charging=false,released=false;
int lastState=0;
std::uint64_t frame=0;
struct Scalar {float base=0,applied=0;bool valid=false;};
struct MaterialState {RE::NiPointer<RE::BSEffectShaderProperty> property;Scalar emission;std::uint64_t seen=0;};
std::unordered_map<RE::BSEffectShaderProperty*,MaterialState> materials;
float Seconds(Clock::time_point time){return std::chrono::duration<float>(Clock::now()-time).count();}
float Apply(Scalar& state,float current,float factor){
    if(!state.valid||std::abs(current-state.applied)>.0001f)state.base=current;
    state.valid=true;state.applied=state.base*factor;return state.applied;
}
float Smooth(float x){x=std::clamp(x,0.0f,1.0f);return x*x*(3-2*x);}
template<class F>void Walk(RE::NiAVObject* object,F&& fn){
    if(!object)return;fn(object);
    if(auto* node=object->AsNode())for(auto& child:node->GetChildren())if(child)Walk(child.get(),fn);
}
int HazardIndex(RE::TESObjectREFR* ref){
    auto* base=ref?ref->GetBaseObject():nullptr;
    for(int i=0;i<2;++i)if(base&&base==hazardTypes[i])return i;
    return -1;
}
void ReadSettings(){
    static Clock::time_point readTime{};if(Seconds(readTime)<.25f)return;readTime=Clock::now();
    for(int i=0;i<Count;++i){
        float v=globals[i]?globals[i]->value:Defaults[i];
        const float next=std::isfinite(v)?std::clamp(v,Minimum[i],Maximum[i]):Defaults[i];
        if(std::abs(next-values[i])>.0001f)SKSE::log::info("Setting {}={}",i,next);
        values[i]=next;
    }
    for(int i=0;i<2;++i)if(hazardTypes[i]){
        if(hazardTypes[i]->data.lifetime!=values[ResidueDuration])hazardTypes[i]->data.lifetime=values[ResidueDuration];
        hazardTypes[i]->data.limit=6;
    }
    if(hazardTypes[1])hazardTypes[1]->data.sound=values[IceSound]>.5f?iceSound:nullptr;
}
void UpdateWorld(RE::NiAVObject* object){
    RE::NiUpdateData data{};data.flags=RE::NiUpdateData::Flag::kDirty;
    object->UpdateWorldData(&data);
    if(auto* node=object->AsNode())for(auto& child:node->GetChildren())if(child)UpdateWorld(child.get());
    object->UpdateWorldBound();
}
struct CoreMaterial {RE::NiPointer<RE::BSLightingShaderProperty> property;float emission=0;};
struct StaffCore {RE::NiPointer<RE::NiAVObject> node;};
std::vector<StaffCore> staffCores;
std::vector<CoreMaterial> coreMaterials;
Clock::time_point coreScan{};
float coreAngle=0,coreGlow=0;
void ClearCores(){
    for(auto& material:coreMaterials)material.property->emissiveMult=material.emission;
    staffCores.clear();coreMaterials.clear();
}
void StaffCores(RE::PlayerCharacter* player,bool active,float delta){
    if(!active&&coreGlow<.001f){if(!staffCores.empty())ClearCores();return;}
    const float charge=charging?std::clamp(Seconds(castStart)/4.0f,0.0f,1.0f):0;
    const float target=active?.5f+2*charge:0;
    coreGlow+=(target-coreGlow)*std::min(delta*10,1.0f);
    if(active)coreAngle=std::fmod(coreAngle+delta*(.8f+charge*1.7f),6.2831853f);
    if(Seconds(coreScan)>=.25f){
        ClearCores();coreScan=Clock::now();
        std::unordered_set<RE::NiAVObject*> nodes;
        std::unordered_set<RE::BSLightingShaderProperty*> properties;
        for(bool first:{false,true})Walk(player->Get3D(first),[&](RE::NiAVObject* node){
            if(std::string_view(node->name.c_str())!="AS_StaffCoreSpin"||!nodes.insert(node).second)return;
            staffCores.push_back({RE::NiPointer<RE::NiAVObject>(node)});
            Walk(node,[&](RE::NiAVObject* child){
                auto* geometry=child->AsGeometry();if(!geometry)return;
                auto* property=geometry->GetGeometryRuntimeData().shaderProperty.get();
                if(!property||property->GetMaterialType()!=RE::BSShaderMaterial::Type::kLighting)return;
                auto* lighting=static_cast<RE::BSLightingShaderProperty*>(property);
                if(lighting->emissiveMult>0&&properties.insert(lighting).second)coreMaterials.push_back({RE::NiPointer<RE::BSLightingShaderProperty>(lighting),lighting->emissiveMult});
            });
        });
    }
    RE::NiMatrix3 spin;spin.MakeYRotation(coreAngle);
    for(auto& core:staffCores)if(core.node->parent){core.node->local.rotate=spin;UpdateWorld(core.node.get());}
    for(auto& material:coreMaterials)material.property->emissiveMult=material.emission*(1+coreGlow*values[Brightness]);
}
void Visuals(RE::NiAVObject* root,bool firstPerson,bool all=false){
    if(!root)return;
    const float elapsed=Seconds(castStart);bool moved=false;
    Walk(root,[&](RE::NiAVObject* object){
        const std::string_view name=object->name.c_str();
        if(!all&&!name.starts_with("AS_"))return;
        if(name=="AS_CastingSize"){
            const float scale=firstPerson?values[FirstPersonScale]:1;
            moved|=object->local.scale!=scale;object->local.scale=scale;
        }
        if(name=="AS_RuneConeRoot"){
            float pulse=1;
            if(values[ReadyPulse]>.5f&&charging&&elapsed>=4)pulse+=.02f*std::sin(elapsed*5.7f)+.012f*std::sin(elapsed*8.63f);
            moved|=object->local.scale!=pulse;object->local.scale=pulse;
            const float since=Seconds(releaseTime);
            const float y=-12-(values[Recoil]>.5f&&since<.26f?4.0f*std::sin(since/.26f*3.14159265f):0);
            moved|=object->local.translate.y!=y;object->local.translate.y=y;
        }
        if(name=="AS_ChargedSpearSpin"){
            float snap=values[AssemblySnap]>.5f&&charging?1.0f-.12f*std::exp(-std::pow((elapsed-3.82f)/.095f,2.0f)):1.0f;
            moved|=object->local.scale!=snap;object->local.scale=snap;
        }
        auto* geometry=object->AsGeometry();if(!geometry)return;
        auto* property=geometry->GetGeometryRuntimeData().shaderProperty.get();
        if(!property||property->GetMaterialType()!=RE::BSShaderMaterial::Type::kEffect)return;
        auto* effect=static_cast<RE::BSEffectShaderProperty*>(property);auto* material=effect->GetMaterial();if(!material)return;
        if(name.starts_with("AS_AirRingGap")){material->baseColorScale=values[BrokenRings]>.5f?0:3.2f*values[Brightness];return;}
        float gain=values[Brightness];
        if(name=="AS_Core"||name=="AS_CentralSeal")gain*=values[CoreBrightness]/.45f;
        if(name.starts_with("AS_FlightMote")&&values[FlightMotes]<.5f)gain=0;
        if(name.starts_with("AS_ImpactEmber")&&values[ImpactEmbers]<.5f)gain=0;
        if(name.starts_with("AS_Fragment")&&Seconds(cancelTime)<.45f&&values[CancelScatter]<.5f)gain=0;
        if(gain==1&&materials.find(effect)==materials.end())return;
        auto& state=materials[effect];state.property.reset(effect);state.seen=frame;
        material->baseColorScale=Apply(state.emission,material->baseColorScale,gain);
    });
    if(moved)UpdateWorld(root);
}
void AddHazard(RE::ObjectRefHandle handle){
    auto ref=handle.get();int type=HazardIndex(ref.get());if(type<0)return;
    auto* current=ref->As<RE::Hazard>();if(!current)return;
    current->GetHazardRuntimeData().lifetime=values[ResidueDuration];
    if(std::find(hazards.begin(),hazards.end(),handle)==hazards.end())hazards.push_back(handle);
}
struct Loaded final:RE::BSTEventSink<RE::TESObjectLoadedEvent>{
    RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* event,RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override {
        if(event&&event->loaded){
            auto* ref=RE::TESForm::LookupByID<RE::TESObjectREFR>(event->formID);
            if(HazardIndex(ref)>=0){auto handle=ref->GetHandle();SKSE::GetTaskInterface()->AddTask([handle]{AddHazard(handle);});}
            else if(ref&&(ref->GetBaseObject()==explosionTypes[0]||ref->GetBaseObject()==explosionTypes[1])){auto handle=ref->GetHandle();SKSE::GetTaskInterface()->AddTask([handle]{effects.push_back(handle);});}
        }
        return RE::BSEventNotifyControl::kContinue;
    }
} loaded;
using PostLoadFn=void(RE::MissileProjectile*,RE::NiAVObject*);
REL::Relocation<PostLoadFn> originalPostLoad;
void PostLoad(RE::MissileProjectile* self,RE::NiAVObject* object){
    originalPostLoad(self,object);
    auto* base=self->GetBaseObject();
    if(base==bolts[0]||base==bolts[1]){
        auto handle=self->GetHandle();SKSE::GetTaskInterface()->AddTask([handle]{projectiles.push_back(handle);});
    }
}
void Combat(){
    if(Seconds(lastCombat)<1)return;lastCombat=Clock::now();
    auto* player=RE::PlayerCharacter::GetSingleton();if(!player||hazards.empty())return;
    if(values[FireDamage]<.5f||!fireSpell)return;
    std::unordered_set<RE::Actor*> hit;
    for(auto handle:hazards){
        auto ground=handle.get();if(HazardIndex(ground.get())!=0)continue;
        RE::TES::GetSingleton()->ForEachReferenceInRange(ground.get(),110,[&](RE::TESObjectREFR* ref){
            auto* actor=ref->As<RE::Actor>();
            if(!actor||actor==player||actor->IsDead()||actor->IsPlayerTeammate()||!actor->IsHostileToActor(player)||!hit.insert(actor).second)return RE::BSContainer::ForEachResult::kContinue;
            auto* caster=player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
            if(caster)caster->CastSpellImmediate(fireSpell,true,actor,1,false,values[FireDamageAmount],player);
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }
}
void Tick(){
    pending=false;if(!enabled)return;++frame;
    ReadSettings();const auto now=Clock::now();const auto delta=now-lastTick;lastTick=now;
    if(auto* ui=RE::UI::GetSingleton();ui&&ui->GameIsPaused()){if(charging)castStart+=delta;releaseTime+=delta;cancelTime+=delta;lastCombat+=delta;return;}
    auto* player=RE::PlayerCharacter::GetSingleton();if(!player||!player->GetParentCell())return;
    if(!player->Get3D()||!player->GetParentCell()->IsAttached())return;
    if(auto* ui=RE::UI::GetSingleton();ui&&ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME))return;
    Transformation::Tick();
#ifdef ASTRAL_CLIP_TRACE
    ClipTrace::Tick(player);
#endif
    auto* caster=player->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand);
    const bool own=caster&&((weapons[0]&&caster->currentSpell==weapons[0]->formEnchanting)||(weapons[1]&&caster->currentSpell==weapons[1]->formEnchanting));
    const int state=own?static_cast<int>(caster->state.get()):0;
    if(state==5&&!charging){charging=true;released=false;castStart=Clock::now();}
    if(state==6&&lastState!=6){releaseTime=Clock::now();released=true;}
    if((state==0||state>=8)&&charging){if(!released)cancelTime=Clock::now();charging=false;}
    lastState=state;
    StaffCores(player,own&&state>=1&&state<=7,std::clamp(std::chrono::duration<float>(delta).count(),0.0f,.1f));
    for(bool first:{false,true})if(auto* root=player->Get3D(first)){
        auto* art=root->GetObjectByName(RE::BSFixedString("AS_Offset"));if(art)Visuals(art,first);
    }
    std::erase_if(projectiles,[&](auto handle){auto ref=handle.get();if(!ref||ref->IsDeleted()||!ref->Get3D())return true;Visuals(ref->Get3D(),false);return false;});
    std::erase_if(effects,[&](auto handle){auto ref=handle.get();if(!ref||ref->IsDeleted()||!ref->Get3D())return true;Visuals(ref->Get3D(),false,true);return false;});
    std::erase_if(hazards,[&](auto handle){
        auto ref=handle.get();auto* hazard=ref?ref->As<RE::Hazard>():nullptr;
        if(!hazard||ref->IsDeleted()){if(ref)hazardVisuals.erase(ref->GetFormID());return true;}
        auto& runtime=hazard->GetHazardRuntimeData();if(runtime.age>=runtime.lifetime){hazardVisuals.erase(ref->GetFormID());return true;}
        if(auto* root=ref->Get3D()){
            if(auto* crystals=root->GetObjectByName(RE::BSFixedString("AS_IceCrystals")))crystals->SetAppCulled(values[IceCrystals]<.5f);
            auto& cached=hazardVisuals[ref->GetFormID()];
            if(cached.root!=root||cached.lifetime!=runtime.lifetime){
                Walk(root,[&](RE::NiAVObject* node){if(std::string_view(node->name.c_str()).starts_with("AS_"))for(auto* control=node->GetControllers();control;control=control->GetNext())control->frequency=5/std::max(runtime.lifetime,.01f);});
                cached={root,runtime.lifetime,frame};
            }
            cached.seen=frame;
            Visuals(root,false,true);
        }
        return false;
    });
    std::erase_if(hazardVisuals,[&](auto& item){return item.second.seen!=frame;});
    Combat();
    std::erase_if(materials,[&](auto& item){return item.second.seen!=frame;});
}
void Reset(){ClearCores();coreGlow=coreAngle=0;coreScan={};hazardVisuals.clear();projectiles.clear();hazards.clear();effects.clear();materials.clear();charging=false;lastState=0;castStart=releaseTime=cancelTime={};}
void Message(SKSE::MessagingInterface::Message* event){
    using M=SKSE::MessagingInterface;
    if(event->type==M::kDataLoaded){
        auto* data=RE::TESDataHandler::GetSingleton();
        for(int i=0;i<2;++i){bolts[i]=data->LookupForm<RE::BGSProjectile>(i?0x825:0x804,"AstralSigil.esp");weapons[i]=data->LookupForm<RE::TESObjectWEAP>(i?0x820:0x800,"AstralSigil.esp");}
        fireSpell=data->LookupForm<RE::SpellItem>(0x8d1,"AstralSigil.esp");
        for(int i=0;i<2;++i)hazardTypes[i]=data->LookupForm<RE::BGSHazard>(0x8c0+i,"AstralSigil.esp");
        for(int i=0;i<Count;++i)globals[i]=data->LookupForm<RE::TESGlobal>(FormIDs[i],Settings::ModuleName);
        for(int i=0;i<2;++i)explosionTypes[i]=data->LookupForm<RE::BGSExplosion>(i?0x827:0x81e,"AstralSigil.esp");
        iceSound=data->LookupForm<RE::BGSSoundDescriptorForm>(0x8d4,"AstralSigil.esp");
        if(!bolts[0]||!bolts[1])return;
        ReadSettings();RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(&loaded);
        REL::Relocation<std::uintptr_t> table{RE::VTABLE_MissileProjectile[0]};originalPostLoad=table.write_vfunc(0xaa,PostLoad);
        SKSE::log::info("Decal lifetime managed by Skyrim");
        Transformation::Initialize();
        enabled=true;
        std::thread([]{while(enabled){std::this_thread::sleep_for(std::chrono::milliseconds(33));if(!pending.exchange(true))SKSE::GetTaskInterface()->AddTask(Tick);}}).detach();
        SKSE::log::info("Settings ready; MCM={}, hazards={},{}",globals[0]!=nullptr,hazardTypes[0]!=nullptr,hazardTypes[1]!=nullptr);
    }else if(event->type==M::kPreLoadGame||event->type==M::kNewGame||event->type==M::kPostLoadGame){Reset();Transformation::Abort();if(event->type==M::kNewGame)Transformation::NewGame();}
}
}
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version=[] {
    SKSE::PluginVersionData data;data.PluginVersion(REL::Version(1,0,0,0));data.PluginName("AstralSigilSettings");data.UsesAddressLibrary();data.UsesUpdatedStructs();
    for(std::size_t i=0;i<Portal::Runtime::Candidates.size();++i){const auto& v=Portal::Runtime::Candidates[i];data.compatibleVersions[i]=REL::Version(v[0],v[1],v[2],v[3]).pack();}return data;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Query(const SKSE::QueryInterface* skse,SKSE::PluginInfo* info){
    info->infoVersion=SKSE::PluginInfo::kVersion;info->name="AstralSigilSettings";info->version=10000;
    const auto v=skse->RuntimeVersion();return !skse->IsEditor()&&Portal::Runtime::Accepts({v[0],v[1],v[2],v[3]});
}
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse){
    const auto version=skse->RuntimeVersion();
    if(skse->IsEditor()||!Portal::Runtime::Accepts({version[0],version[1],version[2],version[3]}))return false;
    SKSE::Init(skse);
    Transformation::InstallSerialization();
    if(auto path=SKSE::log::log_directory()){auto sink=std::make_shared<spdlog::sinks::basic_file_sink_mt>((*path/"AstralSigilSettings.log").string(),true);spdlog::set_default_logger(std::make_shared<spdlog::logger>("AstralSigilSettings",sink));spdlog::flush_on(spdlog::level::info);}
    return SKSE::GetMessagingInterface()->RegisterListener(Message);
}
