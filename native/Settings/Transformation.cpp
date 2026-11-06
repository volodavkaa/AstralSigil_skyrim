#include "Transformation.h"
#include <SKSE/SKSE.h>
#include <RE/S/SendHUDMessage.h>
#include <chrono>
#include <memory>
#include <vector>

namespace Transformation {
namespace {
using Clock=std::chrono::steady_clock;
struct Item {std::uint32_t form=0,owner=0,uid=0;bool operator==(const Item&)const=default;};
struct Pair {Item staff,mace,reservedMelee,reservedLeft;std::uint32_t variant=0,reservedFlags=0;};
static_assert(sizeof(Pair)==56); // Preserve the existing co-save record layout.
std::vector<Pair> pairs;
RE::TESObjectWEAP* staves[2]{};
RE::TESObjectWEAP* maces[2]{};
RE::TESGlobal *key=nullptr,*gripState=nullptr;
RE::BGSEquipSlot *rightSlot=nullptr;
enum class Phase {Idle,Equip,Catch};
struct Transition {Phase phase=Phase::Idle;Item source,target;std::size_t pair=0;bool returning=false,drawn=false;Clock::time_point deadline{},drawnAt{};std::uint32_t leftForm=0;} change;
Clock::time_point lastRequest{};

RE::ExtraDataList* Find(RE::PlayerCharacter* player,const Item& item){
    auto* object=RE::TESForm::LookupByID<RE::TESBoundObject>(item.form);if(!object)return nullptr;
    auto inventory=player->GetInventory([&](RE::TESBoundObject& value){return &value==object;});
    auto entry=inventory.find(object);if(entry==inventory.end()||entry->second.first<1||!entry->second.second->extraLists)return nullptr;
    for(auto* list:*entry->second.second->extraLists)if(list){
        auto* id=list->GetByType<RE::ExtraUniqueID>();
        if(id&&id->baseID==item.owner&&id->uniqueID==item.uid)return list;
    }
    return nullptr;
}
Item Identify(RE::PlayerCharacter* player,RE::TESBoundObject* object,RE::ExtraDataList* list){
    if(!object||!list)return {};
    auto* id=list->GetByType<RE::ExtraUniqueID>();
    if(!id){
        SKSE::log::info("Transformation assigning instance identity {:08X}",object->GetFormID());
        auto* inventory=player->GetInventoryChanges();if(!inventory)return {};
        id=new RE::ExtraUniqueID(player->GetFormID(),inventory->GetNextUniqueID());list->Add(id);
        player->AddChange(RE::TESObjectREFR::ChangeFlags::kInventory);
    }
    return {object->GetFormID(),id->baseID,id->uniqueID};
}
Item Equipped(RE::PlayerCharacter* player,bool left){
    auto* form=player->GetEquippedObject(left);if(!form)return {};
    if(form->As<RE::SpellItem>())return {form->GetFormID(),0,0};
    auto* object=form->As<RE::TESBoundObject>();if(!object)return {};
    auto inventory=player->GetInventory([&](RE::TESBoundObject& value){return &value==object;});
    auto entry=inventory.find(object);if(entry==inventory.end()||entry->second.first<1||!entry->second.second->extraLists)return {};
    for(auto* list:*entry->second.second->extraLists)if(list&&
        (left?list->HasType<RE::ExtraWornLeft>()||(object->As<RE::TESObjectARMO>()&&list->HasType<RE::ExtraWorn>()):list->HasType<RE::ExtraWorn>()))return Identify(player,object,list);
    return {};
}
bool Menu(){
    auto* ui=RE::UI::GetSingleton();return !ui||ui->GameIsPaused()||ui->IsApplicationMenuOpen()||ui->IsItemMenuOpen()||ui->IsModalMenuOpen()||
        ui->IsMenuOpen(RE::Console::MENU_NAME)||ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)||ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
}
bool Busy(RE::PlayerCharacter* player){
    if(!player||!player->Is3DLoaded()||player->IsDead()||player->IsAttacking()||player->IsBlocking()||player->IsStaggering()||player->IsInRagdollState()||player->IsInKillMove()||player->AsActorState()->IsSwimming()||player->AsActorState()->IsBleedingOut()||player->IsInMidair()||player->IsOnMount()||player->AsActorState()->GetSitSleepState()!=RE::SIT_SLEEP_STATE::kNormal)return true;
    for(auto hand:{RE::MagicSystem::CastingSource::kRightHand,RE::MagicSystem::CastingSource::kLeftHand}){
        auto* caster=player->GetMagicCaster(hand);if(caster&&static_cast<int>(caster->state.get())>=1&&static_cast<int>(caster->state.get())<=7)return true;
    }
    return false;
}
void Fail(const char* reason){if(gripState)gripState->value=0;SKSE::log::warn("Transformation aborted: {}",reason);change={};}
struct Input final:RE::BSTEventSink<RE::InputEvent*>{
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,RE::BSTEventSource<RE::InputEvent*>*)override{
        if(!events||!key)return RE::BSEventNotifyControl::kContinue;
        const auto code=static_cast<std::uint32_t>(key->value);
        for(auto* event=*events;event;event=event->next)if(auto* button=event->AsButtonEvent();button&&button->IsDown()&&button->GetDevice()==RE::INPUT_DEVICE::kKeyboard&&button->GetIDCode()==code&&code>0&&code<256)
            SKSE::GetTaskInterface()->AddTask(Request);
        return RE::BSEventNotifyControl::kContinue;
    }
} input;
void Save(SKSE::SerializationInterface* serial){
    for(const auto& pair:pairs)serial->WriteRecord(0x50414952,1,pair);
}
bool Resolve(SKSE::SerializationInterface* serial,Item& item){
    if(!item.form)return true;
    if(!serial->ResolveFormID(item.form,item.form))return false;
    return !item.owner||serial->ResolveFormID(item.owner,item.owner);
}
void Load(SKSE::SerializationInterface* serial){
    pairs.clear();change={};std::uint32_t type,version,length;
    while(serial->GetNextRecordInfo(type,version,length)){
        if(type!=0x50414952||version!=1||length!=sizeof(Pair))continue;
        Pair pair{};if(serial->ReadRecordData(pair)!=sizeof(pair))continue;
        if(pair.variant>1||pairs.size()>=64||!Resolve(serial,pair.staff)||!Resolve(serial,pair.mace))continue;
        pair.reservedMelee={};pair.reservedLeft={};pair.reservedFlags=0;
        pairs.push_back(pair);
    }
    SKSE::log::info("Transformation loaded {} tracked staff pairs",pairs.size());
}
}
void Abort(){if(gripState)gripState->value=0;change={};}
void NewGame(){Abort();pairs.clear();}
void InstallSerialization(){
    auto* serial=SKSE::GetSerializationInterface();serial->SetUniqueID(0x41535446);
    serial->SetSaveCallback(Save);serial->SetLoadCallback(Load);serial->SetRevertCallback([](auto*){NewGame();});
}
void Initialize(){
    auto* data=RE::TESDataHandler::GetSingleton();
    for(int i=0;i<2;++i){
        staves[i]=data->LookupForm<RE::TESObjectWEAP>(i?0x820:0x800,"AstralSigil.esp");
        maces[i]=data->LookupForm<RE::TESObjectWEAP>(0x940+i*2,"AstralSigil.esp");
    }
    key=data->LookupForm<RE::TESGlobal>(0x944,"AstralSigil.esp");gripState=data->LookupForm<RE::TESGlobal>(0x945,"AstralSigil.esp");
    rightSlot=RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x13f42);
    if(!key||!rightSlot||!staves[0]||!staves[1]||!maces[0]||!maces[1])return;
    RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&input);
    SKSE::log::info("Transformation ready: G default; staff / one-handed mace");
}
void Request(){
    SKSE::log::info("Transformation hotkey request");
    auto* player=RE::PlayerCharacter::GetSingleton();auto now=Clock::now();
    if(!key||change.phase!=Phase::Idle||now-lastRequest<std::chrono::milliseconds(500)||Menu()||Busy(player))return;
    auto* current=player->GetEquippedObject(false);if(!current){SKSE::log::info("Transformation ignored: empty right hand");return;}
    auto ws=player->AsActorState()->GetWeaponState();
    if(ws!=RE::WEAPON_STATE::kDrawn){SKSE::log::info("Transformation ignored: weapon not drawn");return;}
    int variant=current==staves[0]?0:current==staves[1]?1:-1;
    bool returning=variant<0;
    if(returning&&current!=maces[0]&&current!=maces[1])return;
    Item source=Equipped(player,false);if(!source.form)return;
    std::size_t index=pairs.size();
    for(std::size_t i=0;i<pairs.size();++i)if(returning?pairs[i].mace==source:pairs[i].staff==source){index=i;break;}
    if(returning&&index==pairs.size())return;
    if(index==pairs.size()){
        if(pairs.size()>=64)return;
        Pair pair{};pair.staff=source;pair.variant=variant;pairs.push_back(pair);
    }
    auto& pair=pairs[index];
    if(!returning&&!pair.mace.form){
        auto* object=maces[pair.variant];
        // Cross-runtime headers expose an opaque ExtraDataList; allocate the
        // largest supported native layout before calling the native constructor.
        auto* memory=RE::malloc<RE::ExtraDataList>(0x20);
        if(!memory)return;
        auto* list=std::construct_at(memory);
        Item created=Identify(player,object,list);if(!created.form){delete list;return;}
        player->AddObjectToContainer(object,list,1,nullptr);pair.mace=created;
    }
    Item target=returning?pair.staff:pair.mace;
    if(!Find(player,target)){SKSE::log::warn("The linked weapon is not in the player inventory");RE::SendHUDMessage::ShowHUDMessage("The linked weapon is not in your inventory.");return;}
    if(gripState)gripState->value=ws==RE::WEAPON_STATE::kDrawn?2.f:0.f;
    lastRequest=now;change={Phase::Equip,source,target,index,returning,ws==RE::WEAPON_STATE::kDrawn,now+std::chrono::seconds(8)};
    auto* left=player->GetEquippedObject(true);change.leftForm=left?left->GetFormID():0;
    SKSE::log::info("Transformation begin {:08X} -> {:08X}, left={:08X}",source.form,target.form,change.leftForm);
    auto* object=RE::TESForm::LookupByID<RE::TESBoundObject>(target.form);
    RE::ActorEquipManager::GetSingleton()->EquipObject(player,object,Find(player,target),1,rightSlot,true,false,false);
}
void Tick(){
    if(change.phase==Phase::Idle)return;
    auto* player=RE::PlayerCharacter::GetSingleton();
    if(Menu()||Busy(player)||Clock::now()>change.deadline||change.pair>=pairs.size()){Fail("unsafe state or timeout");return;}
    auto ws=player->AsActorState()->GetWeaponState();
    if(change.drawn&&(ws==RE::WEAPON_STATE::kSheathing||ws==RE::WEAPON_STATE::kSheathed)){Fail("weapon put away");return;}
    auto* left=player->GetEquippedObject(true);
    if((left?left->GetFormID():0)!=change.leftForm){Fail("left-hand equipment changed");return;}
    auto* source=RE::TESForm::LookupByID(change.source.form);auto* target=RE::TESForm::LookupByID<RE::TESBoundObject>(change.target.form);
    auto* current=player->GetEquippedObject(false);
    if(current!=source&&current!=target){Fail("equipment changed");return;}
    if(change.phase==Phase::Equip){
        if(current!=target)return;
        change.phase=Phase::Catch;
        if(gripState)gripState->value=change.drawn?2.f:0.f;
    }else if(change.phase==Phase::Catch){
        if(change.drawn&&player->AsActorState()->GetWeaponState()!=RE::WEAPON_STATE::kDrawn){
            change.drawnAt={};
            return;
        }
        if(change.drawn){
            if(change.drawnAt==Clock::time_point{})change.drawnAt=Clock::now();
            if(Clock::now()-change.drawnAt<std::chrono::milliseconds(70))return;
        }
        if(change.drawn&&Clock::now()<change.deadline-std::chrono::seconds(8)+std::chrono::milliseconds(200))return;
        SKSE::log::info("Transformation complete {:08X}",change.target.form);if(gripState)gripState->value=0;change={};
    }
}
}
#ifdef ASTRAL_TRANSFORM_DIAGNOSTICS
extern "C" __declspec(dllexport) void AstralSigilTransformTestRequest(){Transformation::Request();}
#endif
