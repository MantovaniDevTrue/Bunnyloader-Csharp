// Host regression: run the production prefix hooks against an IL2CPP fixture
// whose static field reads do NOT execute class constructors (the real ABI).
// This is not a device/Unity integration test.
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <limits>
#include <vector>
#include <thread>

#ifndef PREFIX_GAMEPLAY_SOURCE
#define PREFIX_GAMEPLAY_SOURCE "../app/src/main/cpp/experiments/csharp/ManagedPrefixGameplay.cpp"
#endif
#include PREFIX_GAMEPLAY_SOURCE

struct Il2CppClass { bool initialized = false; };
struct FieldInfo { Il2CppClass* parent; Il2CppArray* value = nullptr; size_t stride; };
struct MethodInfo { int unused = 0; };

namespace {
std::vector<void*> allocations;
std::map<Il2CppArray*, size_t> strides;
Il2CppClass lang, sets, integers;
FieldInfo names{&lang, nullptr, sizeof(void*)}, chances{&sets, nullptr, 1};
MethodInfo prefixMethod, rollableMethod, nextDoubleMethod;
Il2CppObject localized{}, randomObject{};
std::vector<bl::csharp::ManagedPrefixEntry> descriptors;
std::shared_ptr<const std::vector<bl::csharp::ManagedPrefixEntry>> descriptorView;
std::shared_ptr<const std::map<int, bool>> chanceView;
std::shared_ptr<const std::map<int, bl::csharp::ManagedPrefixStackChance>> stackChanceView =
    std::make_shared<const std::map<int, bl::csharp::ManagedPrefixStackChance>>();
std::shared_ptr<const std::map<int, bl::csharp::ManagedPrefixChoice>> choiceView =
    std::make_shared<const std::map<int, bl::csharp::ManagedPrefixChoice>>();
std::shared_ptr<const std::map<int, bl::csharp::ManagedItemPrefixApply>> effectView;
std::shared_ptr<const std::map<int, std::set<int>>> deniedView =
    std::make_shared<const std::map<int, std::set<int>>>();
std::shared_ptr<const bl::csharp::ManagedPrefixStackDenyTable> stackDeniedView =
    std::make_shared<const bl::csharp::ManagedPrefixStackDenyTable>();
std::shared_ptr<const bl::csharp::ManagedPrefixAttributeDenyTable> attributeDeniedView =
    std::make_shared<const bl::csharp::ManagedPrefixAttributeDenyTable>();
std::shared_ptr<const std::map<int, bl::csharp::ManagedItemPrefixCallbackBinding>> itemCallbackView =
    std::make_shared<const std::map<int, bl::csharp::ManagedItemPrefixCallbackBinding>>();
std::shared_ptr<const std::vector<bl::csharp::ManagedGlobalPrefixCallbackBinding>> globalView =
    std::make_shared<const std::vector<bl::csharp::ManagedGlobalPrefixCallbackBinding>>();
std::shared_ptr<const std::map<std::string,std::string>> itemCallbackAssemblies =
    std::make_shared<const std::map<std::string,std::string>>();
int itemWorkerWarmup = 0;
int itemWorkerCalls = 0;
int itemWorkerDenied = 99;
bool itemWorkerFails = false;
int itemChoice = -1;
int itemChance = -1;
int itemPostDamage = 0;
int itemPostValue = 0;
int itemDecisionCalls = 0;
std::shared_ptr<const std::map<std::string,std::string>> prefixAssemblies =
    std::make_shared<const std::map<std::string,std::string>>();
int prefixWorkerWarmup = 0;
int prefixWorkerCalls = 0;
int prefixWorkerCanRoll = 1;
int prefixWorkerWeight = 7;
int prefixWorkerEfficacy = 1;
bool prefixWorkerFails = false;
struct Item {
    Il2CppObject object{};
    int type = 6199, damage = 12, useAnimation = 20, mana = 0, stack = 1, ammo = 0, value = 100;
    float knockBack = 2;
    uint8_t accessory = 0, consumable = 0, ranged = 1, prefix = 0;
};
struct Player { Il2CppObject object{}; int defense = 10; int life = 150; int lifeMax = 200; };
struct ReforgePopup {
    Il2CppObject object{};
    int lines=0;
    Il2CppArray* texts=nullptr;
    Il2CppArray* bad=nullptr;
};
struct Box { Il2CppObject object{}; uint8_t value = 0; } boxed;
struct DoubleBox { Il2CppObject object{}; double value = 0.0; } randomDouble;
bool mixedPrefixCandidates = false;
std::vector<int> batchVanillaCandidates;
int constructorRuns = 0, nativeRolls = 0;

Il2CppArray* allocate(size_t count, size_t stride) {
    auto* p = static_cast<Il2CppArray*>(std::calloc(1, sizeof(Il2CppArray) + count * stride));
    assert(p);
    allocations.push_back(p);
    p->length = count;
    strides[p] = stride;
    return p;
}
void initialize(Il2CppClass* cls) {
    if (cls->initialized) return;
    cls->initialized = true;
    ++constructorRuns;
    for (auto* field : {&names, &chances})
        if (field->parent == cls) field->value = allocate(98, field->stride);
}
void getStatic(FieldInfo* f, void* out) {
    // Storage exists, .cctor has not necessarily run.
    *static_cast<Il2CppArray**>(out) = f->value;
}
void setStatic(FieldInfo* f, void* value) { f->value = static_cast<Il2CppArray*>(value); }
Il2CppArray* vanillaRollable(Il2CppObject* object, const MethodInfo*) {
    if (!reinterpret_cast<Item*>(object)->accessory) return nullptr;
    if (!batchVanillaCandidates.empty()) {
        auto* result=allocate(batchVanillaCandidates.size(),sizeof(int32_t));
        auto* values=static_cast<int32_t*>(arrayData(result));
        std::copy(batchVanillaCandidates.begin(),batchVanillaCandidates.end(),values);
        return result;
    }
    auto* a = allocate(mixedPrefixCandidates ? 2 : 1, sizeof(int32_t));
    auto* values = static_cast<int32_t*>(arrayData(a));
    values[0] = 71;
    if (mixedPrefixCandidates) values[1] = 100;
    return a;
}
bool vanillaRoll(Il2CppObject*, Il2CppObject*, int32_t* result, const MethodInfo*) {
    ++nativeRolls;
    *result = 71;
    return true;
}
int receivedChanceWant = -999;
int chanceOriginalCalls = 0;
int nativePrefixDamage = 12, nativePrefixValue = 100;
bool nativePrefixAccept = true;
bool nativePrefixCumulative = false;
bool vanillaPrefixChance(Il2CppObject* object, int32_t want, uint8_t*, const MethodInfo*) {
    receivedChanceWant = want;
    ++chanceOriginalCalls;
    if (!nativePrefixAccept) return false;
    auto* item = reinterpret_cast<Item*>(object);
    if (want != -3) {
        item->prefix = static_cast<uint8_t>(want > 0 ? want : 71);
        if (!nativePrefixCumulative) {
            item->damage = nativePrefixDamage;
            item->value = nativePrefixValue;
        }
    }
    return true;
}
Il2CppObject* invoke(const MethodInfo* method, void* self, void** args, Il2CppObject** error) {
    using namespace bl::csharp;
    *error = nullptr;
    if (method == &rollableMethod)
        return reinterpret_cast<Il2CppObject*>(hookRollable(static_cast<Il2CppObject*>(self), method));
    if (method == &nextDoubleMethod)
        return &randomDouble.object;
    assert(method == &prefixMethod);
    auto* item = static_cast<Item*>(self);
    assert(*static_cast<int32_t*>(args[0]) == -2);
    *static_cast<uint8_t*>(args[1]) = 0;
    int32_t selected = -1;
    boxed.value = hookRoll(&item->object, &randomObject, &selected, nullptr);
    if (boxed.value) item->prefix = static_cast<uint8_t>(selected);
    return &boxed.object;
}
}

namespace bl::log { void write(int, const char*, ...) {} }
namespace bl::il2cpp { Api& api() { static Api value{}; return value; } }
namespace bl::runtime {
bool isModItem(int type) { return type >= 6196; }
int vanillaPrefixCount() { return 98; }
std::string modPrefixKey(int type) {
    return type == 98 ? "fixture/Guarded" : type == 99 ? "fixture/Mighty" :
           type == 100 ? "other/External" : type == 101 ? "fixture/Custom" : "";
}
Il2CppArray* TypeTables::resizedCopy(Il2CppArray* old, uintptr_t size) {
    const size_t stride = strides.at(old);
    auto* out = allocate(size, stride);
    std::memcpy(arrayData(out), arrayData(old), std::min(size, old->length) * stride);
    return out;
}
namespace content {
Il2CppObject* makeLocalizedText(const std::string&, const std::string&) { return &localized; }
bool setTableElement(FieldInfo* f, int index, Il2CppObject* value) {
    if (static_cast<uintptr_t>(index) >= f->value->length) return false;
    static_cast<Il2CppObject**>(arrayData(f->value))[index] = value;
    return true;
}
}
}
namespace bl::csharp {
std::vector<ManagedPrefixEntry> snapshotManagedPrefixes() { return descriptors; }
std::shared_ptr<const std::vector<ManagedPrefixEntry>> managedPrefixView() { return descriptorView; }
std::shared_ptr<const std::map<int, bool>> managedItemPrefixChanceView() { return chanceView; }
std::shared_ptr<const std::map<int, ManagedPrefixStackChance>> managedItemPrefixStackChanceView() {
    return stackChanceView;
}
std::shared_ptr<const std::map<int, ManagedPrefixChoice>> managedItemPrefixChoiceView() {
    return choiceView;
}
std::shared_ptr<const std::map<int, ManagedItemPrefixApply>> managedItemPrefixApplyView() { return effectView; }
std::shared_ptr<const std::map<int, std::set<int>>> managedItemPrefixDenyView() { return deniedView; }
std::shared_ptr<const ManagedPrefixStackDenyTable> managedItemPrefixStackDenyView() { return stackDeniedView; }
std::shared_ptr<const ManagedPrefixAttributeDenyTable> managedItemPrefixAttributeDenyView() {
    return attributeDeniedView;
}
std::shared_ptr<const std::vector<ManagedGlobalPrefixCallbackBinding>>
managedGlobalPrefixCallbackView() {
    return globalView;
}
std::shared_ptr<const std::map<int, ManagedItemPrefixCallbackBinding>> managedItemPrefixCallbackView() {
    return itemCallbackView;
}
std::shared_ptr<const std::map<std::string,std::string>> managedItemPrefixCallbackAssemblies() {
    return itemCallbackAssemblies;
}
int managedItemPrefixWarmupState(const std::string&,const std::string&,const std::string&) {
    return itemWorkerWarmup;
}
bool invokeManagedItemPrefixDecision(const std::string& mod,const std::string& assembly,
    const std::string& owner,int index,int kind,const std::string& snapshot,
    std::string* output,int budget) {
    assert(mod=="test" && assembly=="/tmp/prefix-fixture.dll" &&
           (owner=="TestWand" || owner=="TestCharm" || owner=="TestGlobal") &&
           (kind==3 || kind==4 || kind==5) &&
           ((kind==3 && index==0) || (kind==4 && index>=1 && index<=2) ||
            (kind==5 && index>=1 && index<=255)) &&
           !snapshot.empty() && budget>=1 && budget<=650);
    ++itemDecisionCalls;
    if(itemWorkerFails)return false;
    if(kind==3) *output=std::to_string(itemChoice);
    if(kind==4) *output=itemChance<0?"N":itemChance==0?"0":"1";
    if(kind==5) *output=std::to_string(itemPostDamage)+","+
                         std::to_string(itemPostValue);
    return true;
}
bool invokeManagedItemPrefixBatch(const std::string& mod,const std::string& assembly,
    const std::string& owner,const std::string& snapshot,size_t count,
    std::string* response,int budget) {
    assert(mod=="test" && assembly=="/tmp/prefix-fixture.dll" &&
           (owner=="TestWand" || owner=="TestCharm" || owner=="TestGlobal") &&
           count>=1 && count<=128 && budget>0 && budget<=650);
    const auto pos=snapshot.find('|');
    assert(pos!=std::string::npos && snapshot.size()-pos-1==count*2);
    ++itemWorkerCalls;
    if (itemWorkerFails) return false;
    response->clear();
    for(size_t i=0;i<count;++i) {
        const int value=std::stoi(snapshot.substr(pos+1+i*2,2),nullptr,16);
        response->push_back(value==itemWorkerDenied?'0':'1');
    }
    return true;
}
bool invokeManagedItemPrefix(const std::string& mod,const std::string& assembly,
                             const std::string& owner,int prefix,const std::string& snapshot,
                             std::string* result,int budget) {
    assert(mod=="test" && assembly=="/tmp/prefix-fixture.dll" &&
           (owner=="TestWand" || owner=="TestCharm" || owner=="TestGlobal") && prefix>=1 && prefix<=255 &&
           (snapshot.rfind("6199,",0)==0 || snapshot.rfind("6196,",0)==0) &&
           budget>0 && budget<=650);
    ++itemWorkerCalls;
    if (itemWorkerFails) return false;
    *result=prefix==itemWorkerDenied ? "0" : "1";
    return true;
}
std::shared_ptr<const std::map<std::string,std::string>> managedPrefixCallbackAssemblies() {
    return prefixAssemblies;
}
int managedPrefixWarmupState(const std::string&,const std::string&,const std::string&) {
    return prefixWorkerWarmup;
}
bool invokeManagedPrefix(const std::string& mod,const std::string& assembly,
                         const std::string& owner,int kind,const std::string& snapshot,
                         std::string* response,int budget) {
    assert(mod=="test" && assembly=="/tmp/prefix-fixture.dll" && owner=="Mighty");
    assert((kind==6 && (snapshot=="150,200" || snapshot=="200,200")) ||
           snapshot=="6199,12,0,1,0,4" || snapshot=="6199,13,0,1,0,4" ||
           (kind==5 && snapshot=="6199,12,0,1,99,4"));
    assert(budget>0 && budget<=650 && kind>=0 && kind<=6 && kind!=2);
    ++prefixWorkerCalls;
    if(prefixWorkerFails) return false;
    if(kind==0) *response=std::to_string(prefixWorkerCanRoll);
    else if(kind==1) *response=std::to_string(prefixWorkerWeight);
    else if(kind==3) *response=std::to_string(prefixWorkerEfficacy);
    else if(kind==4) *response="2,10";
    else if(kind==6) *response=snapshot=="150,200" ? "2" : "-1";
    else *response="0:4F4B;1:5269736B";
    return true;
}
int managedNativeAccessoryDefense(int type) { return type == 6196 ? 7 : 0; }
int managedNativeAccessoryMoveSpeedPercent(int type) { return type == 6200 ? 8 : 0; }
}

int main() {
    using namespace bl::csharp;
    auto& api = bl::il2cpp::api();
    api.field_get_parent = [](FieldInfo* f) { return f->parent; };
    api.runtime_class_init = initialize;
    api.field_static_get_value = getStatic;
    api.field_static_set_value = setStatic;
    api.array_new = [](Il2CppClass*, uintptr_t count) { return allocate(count, sizeof(int32_t)); };
    api.runtime_invoke = invoke;
    g_refs.names = &names;
    g_refs.chances = &chances;
    g_refs.intClass = &integers;
    g_refs.rollableMethod = &rollableMethod;
    g_refs.nextDoubleMethod = &nextDoubleMethod;
#define OFFSET(member) g_refs.member = offsetof(Item, member)
    OFFSET(type); OFFSET(damage); OFFSET(useAnimation); OFFSET(mana); OFFSET(stack);
    OFFSET(ammo); OFFSET(knockBack); OFFSET(accessory); OFFSET(consumable);
    OFFSET(ranged); OFFSET(prefix);
#undef OFFSET
    g_refs.playerDefense = offsetof(Player, defense);
    g_originalRollable = vanillaRollable;
    g_originalRoll = vanillaRoll;
    g_hooksReady = true;
    const auto freshDiagnostics = managedPrefixDiagnostics();
    assert(freshDiagnostics.rollAttempts == 0);
    assert(freshDiagnostics.restoredRolls == 0);
    assert(freshDiagnostics.maxRollMicroseconds == 0);
    ManagedPrefixEntry guarded;
    guarded.name = "Guarded"; guarded.type = 98; guarded.category = 5;
    guarded.rollWeight = 1; guarded.accessoryDefense = 4;
    ManagedPrefixEntry mighty;
    mighty.name = "Mighty"; mighty.type = 99; mighty.category = 4;
    mighty.rollWeight = 1; mighty.multipliers[0] = 1.2f;
    descriptors = {guarded, mighty};
    descriptorView = std::make_shared<const std::vector<ManagedPrefixEntry>>(descriptors);
    assert(managedPrefixView() == descriptorView);

    // Regression: first prefix access, before ANY vanilla natural roll.
    assert(!names.value && !chances.value);
    Item weapon;
    auto* candidates = hookRollable(&weapon.object, nullptr);
    assert(candidates && candidates->length == 1);
    assert(*static_cast<int32_t*>(arrayData(candidates)) == 99);
    assert(constructorRuns == 2 && names.value->length == 100 && chances.value->length == 100);
    // Warm calls must not rerun constructors or lose installed names.
    assert(ensurePrefixTables(descriptors));
    assert(constructorRuns == 2);
    assert(static_cast<Il2CppObject**>(arrayData(names.value))[99] == &localized);

    for (int damage : {12, 18}) {
        weapon.damage = damage; weapon.prefix = 0;
        bool reached = false;
        assert(applyManagedPrefixViaNativeRoll(&weapon.object, 99, &prefixMethod, &reached));
        assert(reached && weapon.prefix == 99 && nativeRolls == 0 && !g_restoreRoll.item);
        float dmg=1, kb=1, spd=1, size=1, shoot=1, mana=1, value=1;
        int crit=0, tag=0, pen=0;
        assert(hookStats(&weapon.object, 99, &dmg, &kb, &spd, &size, &shoot,
                         &mana, &crit, &tag, &pen, &value, nullptr));
        assert(static_cast<int>(std::nearbyint(damage*dmg)) == (damage == 12 ? 14 : 22));
    }
    Item accessory;
    accessory.type=6196; accessory.damage=-1; accessory.accessory=1;
    bool reached=false;
    assert(applyManagedPrefixViaNativeRoll(&accessory.object, 98, &prefixMethod, &reached));
    assert(reached && accessory.prefix==98 && nativeRolls==0);
    Player player;
    hookGrant(&player.object, &accessory.object, nullptr);
    assert(player.defense==14);
    guarded.minItemDamage = 10;
    descriptorView = std::make_shared<const std::vector<ManagedPrefixEntry>>(
        std::vector<ManagedPrefixEntry>{guarded, mighty});
    assert(!hookCanRoll(&accessory.object, 98, nullptr));
    player.defense = 10;
    hookGrant(&player.object, &accessory.object, nullptr);
    assert(player.defense == 14);
    guarded.minItemDamage = 0;
    descriptorView = std::make_shared<const std::vector<ManagedPrefixEntry>>(
        std::vector<ManagedPrefixEntry>{guarded, mighty});
    assert(!applyManagedPrefixViaNativeRoll(&accessory.object, 99, &prefixMethod, &reached));
    assert(!reached && accessory.prefix==98);

    guarded.rollWeight = 3;
    descriptorView = std::make_shared<const std::vector<ManagedPrefixEntry>>(
        std::vector<ManagedPrefixEntry>{guarded, mighty});
    mixedPrefixCandidates = true;
    auto* mixed = hookRollable(&accessory.object, nullptr);
    assert(mixed && mixed->length == 5);
    const auto* mixedIds = static_cast<const int32_t*>(arrayData(mixed));
    assert(mixedIds[0] == 71 && mixedIds[1] == 100);
    for (int i = 2; i < 5; ++i) assert(mixedIds[i] == 98);
    const int priorNativeRolls = nativeRolls;
    choiceView = std::make_shared<const std::map<int, ManagedPrefixChoice>>(
        std::map<int, ManagedPrefixChoice>{{6196, {98, 3, 5}}});
    int32_t selectedChoice = -1;
    accessory.stack = 3;
    randomDouble.value = std::numeric_limits<double>::quiet_NaN();
    assert(hookRoll(&accessory.object, &randomObject, &selectedChoice, nullptr));
    assert(selectedChoice == 98 && nativeRolls == priorNativeRolls);
    accessory.stack = 5;
    assert(hookRoll(&accessory.object, &randomObject, &selectedChoice, nullptr));
    assert(selectedChoice == 98);
    accessory.stack = 2;
    randomDouble.value = 0.1;
    assert(hookRoll(&accessory.object, &randomObject, &selectedChoice, nullptr));
    assert(selectedChoice == 71);
    accessory.stack = 4;
    deniedView = std::make_shared<const std::map<int, std::set<int>>>(
        std::map<int, std::set<int>>{{6196, {98}}});
    assert(hookRoll(&accessory.object, &randomObject, &selectedChoice, nullptr));
    assert(selectedChoice == 71);
    deniedView = std::make_shared<const std::map<int, std::set<int>>>();
    const int choiceStackOffset = g_refs.stack;
    g_refs.stack = -1;
    assert(hookRoll(&accessory.object, &randomObject, &selectedChoice, nullptr));
    assert(selectedChoice == 71);
    g_refs.stack = choiceStackOffset;
    accessory.stack = 2;
    g_restoreRoll = {&accessory.object, 98, false};
    randomDouble.value = std::numeric_limits<double>::quiet_NaN();
    assert(hookRoll(&accessory.object, &randomObject, &selectedChoice, nullptr));
    assert(selectedChoice == 98 && g_restoreRoll.used);
    g_restoreRoll = {};
    choiceView = std::make_shared<const std::map<int, ManagedPrefixChoice>>();
    accessory.stack = 1;
    int32_t selectedMixed = -1;
    randomDouble.value = 0.1;
    assert(hookRoll(&accessory.object, &randomObject, &selectedMixed, nullptr));
    assert(selectedMixed == 71);
    randomDouble.value = 0.3;
    assert(hookRoll(&accessory.object, &randomObject, &selectedMixed, nullptr));
    assert(selectedMixed == 100);
    randomDouble.value = 0.9;
    assert(hookRoll(&accessory.object, &randomObject, &selectedMixed, nullptr));
    assert(selectedMixed == 98);
    assert(nativeRolls == priorNativeRolls);
    const int previousSelection = selectedMixed;
    randomDouble.value = std::numeric_limits<double>::quiet_NaN();
    assert(!hookRoll(&accessory.object, &randomObject, &selectedMixed, nullptr));
    assert(selectedMixed == previousSelection);
    assert(nativeRolls == priorNativeRolls);
    randomDouble.value = 0.5;
    deniedView = std::make_shared<const std::map<int, std::set<int>>>(
        std::map<int, std::set<int>>{{6196, {71}}});
    auto* mixedFiltered = hookRollable(&accessory.object, nullptr);
    assert(mixedFiltered && mixedFiltered->length == 4);
    const auto* filteredIds = static_cast<const int32_t*>(arrayData(mixedFiltered));
    assert(filteredIds[0] == 100);
    for (int i = 1; i < 4; ++i) assert(filteredIds[i] == 98);
    randomDouble.value = 0.1;
    assert(hookRoll(&accessory.object, &randomObject, &selectedMixed, nullptr));
    assert(selectedMixed == 100);
    randomDouble.value = 0.8;
    assert(hookRoll(&accessory.object, &randomObject, &selectedMixed, nullptr));
    assert(selectedMixed == 98);
    assert(nativeRolls == priorNativeRolls);
    deniedView = std::make_shared<const std::map<int, std::set<int>>>();
    stackDeniedView = std::make_shared<const ManagedPrefixStackDenyTable>(
        ManagedPrefixStackDenyTable{{6196, {{71, {3, 5}}}}});
    accessory.stack = 2;
    assert(!isPrefixDenied(&accessory.object, 71));
    auto* normalByStack = hookRollable(&accessory.object, nullptr);
    assert(normalByStack && normalByStack->length == 5);
    accessory.stack = 3;
    assert(isPrefixDenied(&accessory.object, 71));
    auto* filteredByStack = hookRollable(&accessory.object, nullptr);
    assert(filteredByStack && filteredByStack->length == 4);
    const auto* conditionalIds = static_cast<const int32_t*>(arrayData(filteredByStack));
    assert(conditionalIds[0] == 100);
    for (int i = 1; i < 4; ++i) assert(conditionalIds[i] == 98);
    randomDouble.value = 0.1;
    assert(hookRoll(&accessory.object, &randomObject, &selectedMixed, nullptr));
    assert(selectedMixed == 100);
    assert(nativeRolls == priorNativeRolls);
    accessory.stack = 5;
    assert(isPrefixDenied(&accessory.object, 71));
    accessory.stack = 6;
    assert(!isPrefixDenied(&accessory.object, 71));
    const int conditionalStackOffset = g_refs.stack;
    g_refs.stack = -1;
    assert(isPrefixDenied(&accessory.object, 71));
    g_refs.stack = conditionalStackOffset;
    accessory.stack = 1;
    stackDeniedView = std::make_shared<const ManagedPrefixStackDenyTable>();
    attributeDeniedView = std::make_shared<const ManagedPrefixAttributeDenyTable>(
        ManagedPrefixAttributeDenyTable{
            {6196, {{71, {{'D', 10, 25}, {'M', 3, 5}}}}}});
    accessory.damage = 9;
    accessory.mana = 0;
    assert(!isPrefixDenied(&accessory.object, 71));
    auto* allowedByAttribute = hookRollable(&accessory.object, nullptr);
    assert(allowedByAttribute && allowedByAttribute->length > 0);
    bool includesVanillaPrefix = false;
    for (uintptr_t i = 0; i < allowedByAttribute->length; ++i)
        includesVanillaPrefix |=
            static_cast<const int32_t*>(arrayData(allowedByAttribute))[i] == 71;
    assert(includesVanillaPrefix);
    accessory.damage = 10;
    assert(isPrefixDenied(&accessory.object, 71));
    auto* deniedByAttribute = hookRollable(&accessory.object, nullptr);
    assert(deniedByAttribute && deniedByAttribute->length > 0);
    for (uintptr_t i = 0; i < deniedByAttribute->length; ++i)
        assert(static_cast<const int32_t*>(arrayData(deniedByAttribute))[i] != 71);
    accessory.damage = 26;
    assert(!isPrefixDenied(&accessory.object, 71));
    accessory.mana = 3;
    assert(isPrefixDenied(&accessory.object, 71));
    accessory.mana = 6;
    assert(!isPrefixDenied(&accessory.object, 71));
    const int savedManaOffset = g_refs.mana;
    g_refs.mana = -1;
    assert(isPrefixDenied(&accessory.object, 71));
    g_refs.mana = savedManaOffset;
    accessory.damage = 12;
    accessory.mana = 0;
    assert(isPrefixDenied(&accessory.object, 71));
    int32_t attributeSelected = -1;
    randomDouble.value = 0.1;
    assert(hookRoll(&accessory.object, &randomObject, &attributeSelected, nullptr));
    assert(attributeSelected == 100);
    attributeDeniedView = std::make_shared<const ManagedPrefixAttributeDenyTable>();
    mixedPrefixCandidates = false;
    guarded.rollWeight = 1;
    descriptorView = std::make_shared<const std::vector<ManagedPrefixEntry>>(
        std::vector<ManagedPrefixEntry>{guarded, mighty});

    api.string_new = [](const char* text) -> Il2CppString* {
        const size_t length = std::strlen(text);
        void* memory = std::calloc(1, sizeof(Il2CppString) + (length + 1) * sizeof(char16_t));
        allocations.push_back(memory);
        auto* result = static_cast<Il2CppString*>(memory);
        result->length = static_cast<int32_t>(length);
        for (size_t i = 0; i < length; ++i)
            result->chars[i] = static_cast<unsigned char>(text[i]);
        return result;
    };
    api.gc_wbarrier_set_field = [](Il2CppObject*, void** field, void* value) {
        *field = value;
    };
    Il2CppArray* lines = allocate(6, sizeof(Il2CppString*));
    Il2CppArray* modifier = allocate(6, 1);
    Il2CppArray* bad = allocate(6, 1);
    int32_t lineCount = 0;
    g_inventoryTooltipReady = true;
    hookInventoryTooltip(&accessory.object, nullptr, nullptr, nullptr, 0.f,
                         &lineCount, lines, modifier, bad, nullptr, nullptr,
                         nullptr, nullptr);
    assert(lineCount == 2);
    auto** text = static_cast<Il2CppString**>(arrayData(lines));
    assert(text[0] && text[1] && text[0]->length >= 2 && text[1]->length >= 2);
    assert(text[0]->chars[0] == '+' && text[0]->chars[1] == '7');
    assert(text[1]->chars[0] == '+' && text[1]->chars[1] == '4');
    assert(static_cast<uint8_t*>(arrayData(modifier))[0] == 0);
    assert(static_cast<uint8_t*>(arrayData(modifier))[1] == 1);
    accessory.prefix = 0;
    lineCount = 0;
    hookInventoryTooltip(&accessory.object, nullptr, nullptr, nullptr, 0.f,
                         &lineCount, lines, modifier, bad, nullptr, nullptr,
                         nullptr, nullptr);
    assert(lineCount == 1);
    accessory.type = 6199;
    lineCount = 0;
    hookInventoryTooltip(&accessory.object, nullptr, nullptr, nullptr, 0.f,
                         &lineCount, lines, modifier, bad, nullptr, nullptr,
                         nullptr, nullptr);
    assert(lineCount == 0);
    accessory.type = 6200;
    lineCount = 0;
    hookInventoryTooltip(&accessory.object, nullptr, nullptr, nullptr, 0.f,
                         &lineCount, lines, modifier, bad, nullptr, nullptr,
                         nullptr, nullptr);
    assert(lineCount == 1);
    text = static_cast<Il2CppString**>(arrayData(lines));
    assert(text[0] && text[0]->chars[0] == '+' && text[0]->chars[1] == '8');
    assert(static_cast<uint8_t*>(arrayData(modifier))[0] == 0);

    mighty.minItemDamage = 10;
    mighty.maxItemDamage = 25;
    mighty.minItemMana = 0;
    mighty.maxItemMana = 8;
    mighty.minItemStack = 1;
    mighty.maxItemStack = 10;
    mighty.itemWeights[6199] = 3;
    mighty.itemWeights[6198] = 0;
    mighty.hasStackRollWeight = true;
    mighty.minWeightStack = 3;
    mighty.maxWeightStack = 5;
    mighty.weightInsideStackRange = 6;
    descriptors = {guarded, mighty};
    descriptorView = std::make_shared<const std::vector<ManagedPrefixEntry>>(descriptors);
    weapon.type = 6199;
    auto* weighted = hookRollable(&weapon.object, nullptr);
    assert(weighted && weighted->length == 3);
    for (uintptr_t i = 0; i < weighted->length; ++i)
        assert(static_cast<const int32_t*>(arrayData(weighted))[i] == 99);
    assert(hookCanRoll(&weapon.object, 99, nullptr));
    weapon.stack = 3;
    auto* dynamicWeighted = hookRollable(&weapon.object, nullptr);
    assert(dynamicWeighted && dynamicWeighted->length == 6);
    for (uintptr_t i = 0; i < dynamicWeighted->length; ++i)
        assert(static_cast<const int32_t*>(arrayData(dynamicWeighted))[i] == 99);
    assert(hookCanRoll(&weapon.object, 99, nullptr));
    weapon.stack = 5;
    assert(itemRollWeight(&weapon.object, mighty) == 6);
    weapon.stack = 6;
    assert(itemRollWeight(&weapon.object, mighty) == 3);
    mighty.hasStackRollWeight = false;
    mighty.hasAttributeRollWeight = true;
    mighty.attributeWeightKind = 'D';
    mighty.minWeightAttribute = 15;
    mighty.maxWeightAttribute = 20;
    mighty.weightInsideAttributeRange = 7;
    weapon.damage = 12;
    assert(itemRollWeight(&weapon.object, mighty) == 3);
    weapon.damage = 15;
    assert(itemRollWeight(&weapon.object, mighty) == 7);
    weapon.damage = 20;
    assert(itemRollWeight(&weapon.object, mighty) == 7);
    weapon.damage = 21;
    assert(itemRollWeight(&weapon.object, mighty) == 3);
    mighty.weightInsideAttributeRange = 0;
    weapon.damage = 17;
    assert(itemRollWeight(&weapon.object, mighty) == 0);
    descriptorView = std::make_shared<const std::vector<ManagedPrefixEntry>>(
        std::vector<ManagedPrefixEntry>{guarded, mighty});
    assert(!hookCanRoll(&weapon.object, 99, nullptr));
    mighty.attributeWeightKind = 'M';
    mighty.minWeightAttribute = 2;
    mighty.maxWeightAttribute = 8;
    mighty.weightInsideAttributeRange = 5;
    weapon.damage = 12;
    weapon.mana = 0;
    assert(itemRollWeight(&weapon.object, mighty) == 3);
    weapon.mana = 2;
    assert(itemRollWeight(&weapon.object, mighty) == 5);
    weapon.mana = 8;
    assert(itemRollWeight(&weapon.object, mighty) == 5);
    weapon.mana = 9;
    assert(itemRollWeight(&weapon.object, mighty) == 3);
    const int nativeManaOffsetForWeight = g_refs.mana;
    g_refs.mana = -1;
    assert(itemRollWeight(&weapon.object, mighty) == 0);
    g_refs.mana = nativeManaOffsetForWeight;
    mighty.hasAttributeRollWeight = false;
    mighty.hasStackRollWeight = true;
    weapon.mana = 0;
    mighty.weightInsideStackRange = 0;
    descriptorView = std::make_shared<const std::vector<ManagedPrefixEntry>>(
        std::vector<ManagedPrefixEntry>{guarded, mighty});
    weapon.stack = 4;
    assert(!hookCanRoll(&weapon.object, 99, nullptr));
    assert(hookRollable(&weapon.object, nullptr) == nullptr);
    mighty.weightInsideStackRange = 6;
    descriptorView = std::make_shared<const std::vector<ManagedPrefixEntry>>(
        std::vector<ManagedPrefixEntry>{guarded, mighty});
    weapon.stack = 1;
    assert(hookCanRoll(&weapon.object, 99, nullptr));
    weapon.damage = 9;
    assert(!hookCanRoll(&weapon.object, 99, nullptr));
    assert(hookRollable(&weapon.object, nullptr) == nullptr);
    weapon.damage = 26;
    assert(!hookCanRoll(&weapon.object, 99, nullptr));
    weapon.damage = 25;
    assert(hookCanRoll(&weapon.object, 99, nullptr));
    weapon.damage = 10;
    assert(hookCanRoll(&weapon.object, 99, nullptr));
    weapon.damage = 12;
    weapon.mana = 9;
    assert(!hookCanRoll(&weapon.object, 99, nullptr));
    weapon.mana = 8;
    assert(hookCanRoll(&weapon.object, 99, nullptr));
    weapon.stack = 0;
    assert(!hookCanRoll(&weapon.object, 99, nullptr));
    assert(hookRollable(&weapon.object, nullptr) == nullptr);
    weapon.stack = 10;
    assert(hookCanRoll(&weapon.object, 99, nullptr));
    const int nativeStackOffset = g_refs.stack;
    g_refs.stack = -1;
    assert(!hookCanRoll(&weapon.object, 99, nullptr));
    mighty.minItemStack = 0;
    mighty.maxItemStack = 999999;
    mighty.hasStackRollWeight = false;
    descriptorView = std::make_shared<const std::vector<ManagedPrefixEntry>>(
        std::vector<ManagedPrefixEntry>{guarded, mighty});
    assert(hookCanRoll(&weapon.object, 99, nullptr));
    mighty.hasStackRollWeight = true;
    mighty.minItemStack = 1;
    mighty.maxItemStack = 10;
    descriptorView = std::make_shared<const std::vector<ManagedPrefixEntry>>(
        std::vector<ManagedPrefixEntry>{guarded, mighty});
    g_refs.stack = nativeStackOffset;
    weapon.stack = 11;
    assert(!hookCanRoll(&weapon.object, 99, nullptr));
    weapon.stack = 1;
    assert(hookCanRoll(&weapon.object, 99, nullptr));
    weapon.damage = 9;
    weapon.mana = 9;
    weapon.stack = 11;
    weapon.prefix = 0;
    assert(!hookCanRoll(&weapon.object, 99, nullptr));
    g_restoreRoll = {&weapon.object, 99, false};
    assert(hookCanRoll(&weapon.object, 99, nullptr));
    float restoreDamage=1, restoreKnockback=1, restoreSpeed=1;
    float restoreSize=1, restoreShoot=1, restoreMana=1, restoreValue=1;
    int restoreCrit=0, restoreTag=0, restorePen=0;
    assert(hookStats(&weapon.object, 99, &restoreDamage, &restoreKnockback,
                     &restoreSpeed, &restoreSize, &restoreShoot, &restoreMana,
                     &restoreCrit, &restoreTag, &restorePen, &restoreValue, nullptr));
    g_restoreRoll = {};
    bool restoredDespiteNewConditions = false;
    assert(applyManagedPrefixViaNativeRoll(&weapon.object, 99,
                                            &prefixMethod, &restoredDespiteNewConditions));
    assert(restoredDespiteNewConditions && weapon.prefix == 99);
    assert(!g_restoreRoll.item);
    assert(!hookCanRoll(&weapon.object, 99, nullptr));
    weapon.prefix = 0;
    weapon.damage = 12;
    weapon.mana = 0;
    weapon.stack = 1;
    weapon.type = 6198;
    assert(hookRollable(&weapon.object, nullptr) == nullptr);
    assert(!hookCanRoll(&weapon.object, 99, nullptr));
    weapon.type = 6199;
    assert(itemRollWeight(&weapon.object, mighty) == 3);
    assert(itemRollWeight(&accessory.object, mighty) == 1);
    mighty.itemCanRoll[6199] = false;
    mighty.itemCanRoll[6198] = true;
    descriptors = {guarded, mighty};
    descriptorView = std::make_shared<const std::vector<ManagedPrefixEntry>>(descriptors);
    weapon.type = 6199;
    assert(!hookCanRoll(&weapon.object, 99, nullptr));
    assert(hookRollable(&weapon.object, nullptr) == nullptr);
    weapon.type = 6198;
    assert(hookCanRoll(&weapon.object, 99, nullptr));
    assert(hookRollable(&weapon.object, nullptr) == nullptr);
    bool reachedZeroWeight = false;
    assert(applyManagedPrefixViaNativeRoll(&weapon.object, 99,
                                           &prefixMethod, &reachedZeroWeight));
    assert(reachedZeroWeight && weapon.prefix == 99);
    weapon.prefix = 0;
    mighty.itemCanRoll.erase(6199);

    ManagedPrefixEntry custom;
    custom.name = "Custom";
    custom.type = 101;
    custom.category = 6;
    custom.rollWeight = 1;
    custom.itemWeights[6199] = 0;
    custom.itemCanRoll[6199] = true;
    custom.preferredTypes.insert(6199);
    custom.itemWeights[6198] = 0;
    descriptors = {guarded, mighty, custom};
    descriptorView = std::make_shared<const std::vector<ManagedPrefixEntry>>(descriptors);
    weapon.type = 6199;
    weapon.prefix = 0;
    assert(hookCanRoll(&weapon.object, 101, nullptr));
    auto* customRollable = hookRollable(&weapon.object, nullptr);
    assert(customRollable && customRollable->length == 3);
    for (uintptr_t i = 0; i < customRollable->length; ++i)
        assert(static_cast<const int32_t*>(arrayData(customRollable))[i] == 99);
    int32_t autoChosen = -1;
    assert(hookRoll(&weapon.object, &randomObject, &autoChosen, nullptr));
    assert(autoChosen == 101 && nativeRolls == 0);
    bool reachedCustom = false;
    assert(applyManagedPrefixViaNativeRoll(&weapon.object, 101,
                                           &prefixMethod, &reachedCustom));
    assert(reachedCustom && weapon.prefix == 101 && nativeRolls == 0);
    weapon.type = 6198;
    weapon.prefix = 0;
    assert(!hookCanRoll(&weapon.object, 101, nullptr));
    assert(!applyManagedPrefixViaNativeRoll(&weapon.object, 101,
                                            &prefixMethod, &reachedCustom));
    assert(!reachedCustom && weapon.prefix == 0);

    g_originalItemPrefix = vanillaPrefixChance;
    uint8_t beforeChanceTier = 0;
    weapon.type = 6199;
    assert(!g_chanceReady.load(std::memory_order_acquire));
    assert(hookItemPrefixChance(&weapon.object, -1, &beforeChanceTier, nullptr));
    assert(receivedChanceWant == -1);
    g_chanceItemTypeOffset = offsetof(Item, type);
    g_chancePrefixOffset = offsetof(Item, prefix);
    g_chanceDamageOffset = offsetof(Item, damage);
    g_chanceValueOffset = offsetof(Item, value);
    g_chanceReady.store(true, std::memory_order_release);
    chanceView = std::make_shared<const std::map<int, bool>>(
        std::map<int, bool>{{6199, false}, {6198, true}});
    effectView = std::make_shared<const std::map<int, ManagedItemPrefixApply>>(
        std::map<int, ManagedItemPrefixApply>{{6199, {4, 125}}, {6198, {10, 200}}});
    uint8_t chanceTier = 0;
    weapon.type = 6199;
    int originalCalls = chanceOriginalCalls;
    assert(!hookItemPrefixChance(&weapon.object, -1, &chanceTier, nullptr));
    assert(!hookItemPrefixChance(&weapon.object, -2, &chanceTier, nullptr));
    assert(chanceOriginalCalls == originalCalls);
    g_restoreRoll = {&weapon.object, 99, false};
    assert(hookItemPrefixChance(&weapon.object, -2, &chanceTier, nullptr));
    assert(receivedChanceWant == -2);
    assert(weapon.damage == 16 && weapon.value == 225);
    g_restoreRoll = {};
    assert(hookItemPrefixChance(&weapon.object, 99, &chanceTier, nullptr));
    assert(receivedChanceWant == 99 && chanceOriginalCalls == originalCalls + 2);
    assert(weapon.prefix == 99 && weapon.damage == 16 && weapon.value == 225);
    weapon.type = 6198;
    assert(hookItemPrefixChance(&weapon.object, -1, &chanceTier, nullptr));
    assert(receivedChanceWant == -2 && weapon.damage == 22 && weapon.value == 300);
    assert(hookItemPrefixChance(&weapon.object, -2, &chanceTier, nullptr));
    assert(receivedChanceWant == -2);
    weapon.type = 6200;
    assert(hookItemPrefixChance(&weapon.object, -1, &chanceTier, nullptr));
    assert(weapon.damage == 12 && weapon.value == 100);
    assert(receivedChanceWant == -1);
    chanceView = std::make_shared<const std::map<int, bool>>();
    assert(hookItemPrefixChance(&weapon.object, -1, &chanceTier, nullptr));
    assert(receivedChanceWant == -1);
    stackChanceView = std::make_shared<const std::map<int, ManagedPrefixStackChance>>(
        std::map<int, ManagedPrefixStackChance>{{6199, {3, 5, true}},
                                                {6198, {3, 5, false}}});
    Item stackChanceItem{};
    stackChanceItem.type = 6199;
    stackChanceItem.stack = 2;
    assert(hookItemPrefixChance(&stackChanceItem.object, -1, &chanceTier, nullptr));
    assert(receivedChanceWant == -1);
    stackChanceItem.stack = 3;
    assert(hookItemPrefixChance(&stackChanceItem.object, -1, &chanceTier, nullptr));
    assert(receivedChanceWant == -2);
    stackChanceItem.stack = 5;
    assert(hookItemPrefixChance(&stackChanceItem.object, -2, &chanceTier, nullptr));
    assert(receivedChanceWant == -2);
    stackChanceItem.stack = 6;
    assert(hookItemPrefixChance(&stackChanceItem.object, -1, &chanceTier, nullptr));
    assert(receivedChanceWant == -1);
    stackChanceItem.stack = 4;
    stackChanceItem.type = 6198;
    const int beforeReject = chanceOriginalCalls;
    assert(!hookItemPrefixChance(&stackChanceItem.object, -1, &chanceTier, nullptr));
    assert(chanceOriginalCalls == beforeReject);
    stackChanceItem.type = 6199;
    const int stackChanceField = g_refs.stack;
    g_refs.stack = -1;
    assert(!hookItemPrefixChance(&stackChanceItem.object, -1, &chanceTier, nullptr));
    assert(chanceOriginalCalls == beforeReject);
    g_restoreRoll = {&stackChanceItem.object, 99, false};
    assert(hookItemPrefixChance(&stackChanceItem.object, -2, &chanceTier, nullptr));
    g_restoreRoll = {};
    g_refs.stack = stackChanceField;
    stackChanceView = std::make_shared<const std::map<int, ManagedPrefixStackChance>>();
    weapon.type = 6199;
    nativePrefixAccept = false;
    weapon.damage = 42;
    weapon.value = 133;
    assert(!hookItemPrefixChance(&weapon.object, 99, &chanceTier, nullptr));
    assert(weapon.damage == 42 && weapon.value == 133);
    nativePrefixAccept = true;
    nativePrefixDamage = INT32_MAX - 2;
    assert(hookItemPrefixChance(&weapon.object, 99, &chanceTier, nullptr));
    assert(weapon.damage == INT32_MAX - 2 && weapon.value == 100);
    nativePrefixDamage = 12;
    weapon.damage = 27;
    weapon.value = 77;
    weapon.prefix = 0;
    assert(hookItemPrefixChance(&weapon.object, -3, &chanceTier, nullptr));
    assert(weapon.damage == 27 && weapon.value == 77 && weapon.prefix == 0);
    effectView = std::make_shared<const std::map<int, ManagedItemPrefixApply>>();
    assert(hookItemPrefixChance(&weapon.object, 99, &chanceTier, nullptr));
    assert(weapon.damage == 12 && weapon.value == 100);
    effectView = std::make_shared<const std::map<int, ManagedItemPrefixApply>>(
        std::map<int, ManagedItemPrefixApply>{{6199, {4, 125}}});
    chanceView = std::make_shared<const std::map<int, bool>>();
    nativePrefixCumulative = true;
    weapon.type = 6199;
    weapon.damage = 12;
    weapon.value = 100;
    weapon.prefix = 0;
    assert(hookItemPrefixChance(&weapon.object, 99, &chanceTier, nullptr));
    assert(weapon.damage == 16 && weapon.value == 225);
    for (int i = 0; i < 4; ++i) {
        assert(hookItemPrefixChance(&weapon.object, 99, &chanceTier, nullptr));
        assert(weapon.damage == 16 && weapon.value == 225);
    }
    nativePrefixAccept = false;
    assert(!hookItemPrefixChance(&weapon.object, 99, &chanceTier, nullptr));
    assert(weapon.damage == 16 && weapon.value == 225);
    nativePrefixAccept = true;
    assert(hookItemPrefixChance(&weapon.object, 99, &chanceTier, nullptr));
    assert(weapon.damage == 16 && weapon.value == 225);
    Item other{};
    other.type = 6199;
    assert(hookItemPrefixChance(&other.object, 99, &chanceTier, nullptr));
    assert(other.damage == 16 && other.value == 225);
    assert(hookItemPrefixChance(&weapon.object, 99, &chanceTier, nullptr));
    assert(weapon.damage == 16 && weapon.value == 225);
    weapon.damage = 70;
    weapon.value = 900;
    assert(hookItemPrefixChance(&weapon.object, 99, &chanceTier, nullptr));
    assert(weapon.damage == 74 && weapon.value == 1025);
    nativePrefixCumulative = false;
    nativePrefixValue = INT32_MAX - 100;
    assert(hookItemPrefixChance(&weapon.object, 99, &chanceTier, nullptr));
    assert(weapon.damage == 12 && weapon.value == INT32_MAX - 100);
    nativePrefixValue = 100;

    mighty.applyDamageBonus = 3;
    mighty.applyValueBonus = 40;
    mighty.requireEffectiveStats = false;
    mighty.extraTooltip = "Bonus adicional do prefixo";
    mighty.tooltipLines = {"First line", "Second line"};
    descriptors = {guarded, mighty, custom};
    descriptorView = std::make_shared<const std::vector<ManagedPrefixEntry>>(descriptors);
    Item extra{};
    extra.type = 6199;
    extra.damage = 12;
    extra.value = 100;
    effectView = std::make_shared<const std::map<int, ManagedItemPrefixApply>>(
        std::map<int, ManagedItemPrefixApply>{{6199, {4, 125}}});
    nativePrefixCumulative = true;
    assert(hookItemPrefixChance(&extra.object, 99, &chanceTier, nullptr));
    assert(extra.damage == 19 && extra.value == 265);
    for (int i = 0; i < 4; ++i) {
        assert(hookItemPrefixChance(&extra.object, 99, &chanceTier, nullptr));
        assert(extra.damage == 19 && extra.value == 265);
    }
    effectView = std::make_shared<const std::map<int, ManagedItemPrefixApply>>();
    Item onlyPrefix{};
    onlyPrefix.type = 6199;
    onlyPrefix.damage = 12;
    onlyPrefix.value = 100;
    assert(hookItemPrefixChance(&onlyPrefix.object, 99, &chanceTier, nullptr));
    assert(onlyPrefix.damage == 15 && onlyPrefix.value == 140);
    assert(onlyPrefix.prefix == 99);
    assert(hookItemPrefixChance(&onlyPrefix.object, 99, &chanceTier, nullptr));
    assert(onlyPrefix.damage == 15 && onlyPrefix.value == 140);
    nativePrefixCumulative = false;
    Item tooltipItem{};
    tooltipItem.type = 6199;
    tooltipItem.prefix = 99;
    lineCount = 0;
    hookInventoryTooltip(&tooltipItem.object, nullptr, nullptr, nullptr, 0.f,
                         &lineCount, lines, modifier, bad, nullptr, nullptr,
                         nullptr, nullptr);
    assert(lineCount == 3);
    auto** prefixTooltip = static_cast<Il2CppString**>(arrayData(lines));
    assert(prefixTooltip[0] && prefixTooltip[0]->length > 5);
    assert(prefixTooltip[0]->chars[0] == 'B');
    assert(prefixTooltip[1] && prefixTooltip[1]->chars[0] == 'F');
    assert(prefixTooltip[1]->length == 10);
    assert(prefixTooltip[2] && prefixTooltip[2]->chars[0] == 'S');
    for (int index = 0; index < 3; ++index) {
        assert(static_cast<uint8_t*>(arrayData(modifier))[index] == 1);
        assert(static_cast<uint8_t*>(arrayData(bad))[index] == 0);
    }
    lineCount = 5;
    hookInventoryTooltip(&tooltipItem.object, nullptr, nullptr, nullptr, 0.f,
                         &lineCount, lines, modifier, bad, nullptr, nullptr,
                         nullptr, nullptr);
    assert(lineCount == 6);
    assert(static_cast<Il2CppString**>(arrayData(lines))[5]->chars[0] == 'B');
    mighty.tooltipItemLines = {"D:{damage} M:{mana} S:{stack}"};
    descriptorView = std::make_shared<const std::vector<ManagedPrefixEntry>>(
        std::vector<ManagedPrefixEntry>{guarded, mighty, custom});
    tooltipItem.damage = 17;
    tooltipItem.mana = 4;
    tooltipItem.stack = 6;
    lineCount = 0;
    hookInventoryTooltip(&tooltipItem.object, nullptr, nullptr, nullptr, 0.f,
                         &lineCount, lines, modifier, bad, nullptr, nullptr,
                         nullptr, nullptr);
    assert(lineCount == 4);
    auto* dynamicLine = static_cast<Il2CppString**>(arrayData(lines))[3];
    assert(dynamicLine && utf8Text(dynamicLine) == "D:17 M:4 S:6");
    tooltipItem.damage = 25;
    tooltipItem.mana = 8;
    tooltipItem.stack = 1;
    lineCount = 0;
    hookInventoryTooltip(&tooltipItem.object, nullptr, nullptr, nullptr, 0.f,
                         &lineCount, lines, modifier, bad, nullptr, nullptr,
                         nullptr, nullptr);
    assert(lineCount == 4);
    dynamicLine = static_cast<Il2CppString**>(arrayData(lines))[3];
    assert(dynamicLine && utf8Text(dynamicLine) == "D:25 M:8 S:1");
    const int tooltipManaOffset = g_refs.mana;
    g_refs.mana = -1;
    lineCount = 0;
    hookInventoryTooltip(&tooltipItem.object, nullptr, nullptr, nullptr, 0.f,
                         &lineCount, lines, modifier, bad, nullptr, nullptr,
                         nullptr, nullptr);
    assert(lineCount == 3);
    g_refs.mana = tooltipManaOffset;
    mighty.tooltipItemLines.clear();
    descriptorView = std::make_shared<const std::vector<ManagedPrefixEntry>>(
        std::vector<ManagedPrefixEntry>{guarded, mighty, custom});
    Item ineffective{};
    ineffective.type = 6199;
    ineffective.damage = 12;
    ineffective.useAnimation = 0;
    float d=1, k=1, sp=1, si=1, sh=1, ma=1, val=1;
    int cr=0, tag=0, penetration=0;
    assert(hookStats(&ineffective.object, 99, &d, &k, &sp, &si, &sh,
                     &ma, &cr, &tag, &penetration, &val, nullptr));

    deniedView = std::make_shared<const std::map<int, std::set<int>>>(
        std::map<int, std::set<int>>{{6199, {71, 99}}});
    Item rejected{};
    rejected.type = 6199;
    rejected.accessory = 1;
    auto* candidatesDenied = hookRollable(&rejected.object, nullptr);
    assert(candidatesDenied && candidatesDenied->length == 1);
    assert(*static_cast<int32_t*>(arrayData(candidatesDenied)) == 98);
    assert(!hookCanRoll(&rejected.object, 99, nullptr));
    assert(hookCanRoll(&rejected.object, 98, nullptr));
    const int previousCalls = chanceOriginalCalls;
    assert(!hookItemPrefixChance(&rejected.object, 99, &chanceTier, nullptr));
    assert(chanceOriginalCalls == previousCalls);
    g_restoreRoll = {&rejected.object, 99, false};
    assert(hookItemPrefixChance(&rejected.object, 99, &chanceTier, nullptr));
    assert(chanceOriginalCalls == previousCalls + 1);
    g_restoreRoll = {};
    rejected.accessory = 0;
    assert(hookRollable(&rejected.object, nullptr) == nullptr);
    auto oldView = descriptorView;
    descriptorView = std::make_shared<const std::vector<ManagedPrefixEntry>>();
    rejected.accessory = 1;
    const int vanillaAttempts = nativeRolls;
    int32_t none = 0;
    assert(hookRollable(&rejected.object, nullptr) == nullptr);
    assert(!hookRoll(&rejected.object, &randomObject, &none, nullptr));
    assert(nativeRolls == vanillaAttempts);
    descriptorView = oldView;
    deniedView = std::make_shared<const std::map<int, std::set<int>>>();
    assert(hookCanRoll(&rejected.object, 99, nullptr));

    // Language/table regeneration must be handled even after the first success.
    names.value=allocate(98,sizeof(void*));
    chances.value=allocate(98,1);
    assert(ensurePrefixTables(descriptors));
    assert(names.value->length==102 && chances.value->length==102 && constructorRuns==2);
    // Genuine unavailability is not turned into a fake successful restoration.
    auto saved=api.runtime_class_init;
    api.runtime_class_init=nullptr;
    assert(!ensurePrefixTables(descriptors));
    api.runtime_class_init=saved;
    g_hooksReady.store(false, std::memory_order_release);
    int32_t forcedFallback = -1;
    assert(hookRoll(&rejected.object, &randomObject, &forcedFallback, nullptr));
    assert(forcedFallback == 71);
    g_hooksReady.store(true, std::memory_order_release);
    prefixAssemblies=std::make_shared<const std::map<std::string,std::string>>(
        std::map<std::string,std::string>{{"test","/tmp/prefix-fixture.dll"}});
    prefixWorkerWarmup=1;
    weapon.type=6199;weapon.damage=12;weapon.mana=0;weapon.stack=1;
    weapon.prefix=0;weapon.ranged=1;weapon.accessory=0;weapon.consumable=0;
    ManagedPrefixEntry livePrefix;
    livePrefix.modId="test";livePrefix.name="Mighty";livePrefix.type=99;
    livePrefix.category=4;livePrefix.rollWeight=1;livePrefix.callbackMask=3;
    prefixWorkerCalls=0;prefixWorkerWeight=7;prefixWorkerCanRoll=1;
    {
        PrefixRpcScope scope;
        assert(itemRollWeight(&weapon.object,livePrefix)==7);
        assert(itemRollWeight(&weapon.object,livePrefix)==7);
        assert(prefixWorkerCalls==1);
        assert(eligible(&weapon.object,livePrefix,true));
        assert(eligible(&weapon.object,livePrefix,true));
        assert(prefixWorkerCalls==2);
    }
    prefixWorkerCanRoll=0;
    {
        PrefixRpcScope scope;
        assert(!eligible(&weapon.object,livePrefix,true));
        assert(prefixWorkerCalls==4);
    }
    prefixWorkerWarmup=0;
    {
        PrefixRpcScope scope;
        assert(itemRollWeight(&weapon.object,livePrefix)==1);
        assert(eligible(&weapon.object,livePrefix,true));
        assert(prefixWorkerCalls==4);
    }
    prefixWorkerWarmup=1;
    prefixWorkerFails=true;
    {
        PrefixRpcScope scope;
        assert(itemRollWeight(&weapon.object,livePrefix)==1);
        assert(eligible(&weapon.object,livePrefix,true));
        assert(prefixWorkerCalls==6);
    }
    prefixWorkerFails=false;
    livePrefix.callbackMask=15;
    prefixWorkerEfficacy=1;
    {
        PrefixRpcScope scope;
        assert(queryPrefixWorker(&weapon.object,livePrefix,3)==1);
        assert(queryPrefixWorker(&weapon.object,livePrefix,3)==1);
        assert(prefixWorkerCalls==7);
    }
    prefixWorkerEfficacy=0;
    {
        PrefixRpcScope scope;
        assert(queryPrefixWorker(&weapon.object,livePrefix,3)==0);
        assert(prefixWorkerCalls==8);
    }
    prefixWorkerEfficacy=1;
    int dynamicDamage=-1,dynamicValue=-1;
    assert(queryPrefixApply(&weapon.object,livePrefix,&dynamicDamage,&dynamicValue));
    assert(dynamicDamage==2 && dynamicValue==10 && prefixWorkerCalls==9);
    prefixWorkerFails=true;
    assert(!queryPrefixApply(&weapon.object,livePrefix,&dynamicDamage,&dynamicValue));
    assert(prefixWorkerCalls==10);
    prefixWorkerFails=false;
    livePrefix.callbackMask=31;
    {
        auto lines=queryPrefixTooltip(&weapon.object,livePrefix);
        assert(lines.size()==2 && lines[0].first=="OK" && !lines[0].second);
        assert(lines[1].first=="Risk" && lines[1].second);
        assert(prefixWorkerCalls==11);
        lines=queryPrefixTooltip(&weapon.object,livePrefix);
        assert(lines.size()==2 && prefixWorkerCalls==11);
    }
    descriptorView=std::make_shared<const std::vector<ManagedPrefixEntry>>(
        std::vector<ManagedPrefixEntry>{guarded,livePrefix});
    ReforgePopup reforge;
    reforge.texts=allocate(6,sizeof(Il2CppString*));
    reforge.bad=allocate(6,1);
    g_refs.reforgeNumLines=offsetof(ReforgePopup,lines);
    g_refs.reforgeText=offsetof(ReforgePopup,texts);
    g_refs.reforgeBad=offsetof(ReforgePopup,bad);
    g_reforgeTooltipReady=true;
    weapon.prefix=99;
    hookReforgeTooltip(&reforge.object,&weapon.object,nullptr);
    assert(reforge.lines==2 && prefixWorkerCalls==12);
    auto** reforgeText=static_cast<Il2CppString**>(arrayData(reforge.texts));
    assert(reforgeText[0] && reforgeText[1]);
    assert(reforgeText[0]->chars[0]=='O' && reforgeText[1]->chars[0]=='R');
    assert(static_cast<uint8_t*>(arrayData(reforge.bad))[0]==0);
    assert(static_cast<uint8_t*>(arrayData(reforge.bad))[1]==1);
    g_reforgeTooltipReady=false;
    weapon.prefix=0;
    ManagedPrefixEntry liveAccessory=guarded;
    liveAccessory.modId="test";
    liveAccessory.name="Mighty";
    liveAccessory.callbackMask=32;
    descriptorView=std::make_shared<const std::vector<ManagedPrefixEntry>>(
        std::vector<ManagedPrefixEntry>{liveAccessory,livePrefix});
    g_refs.playerLife=offsetof(Player,life);
    g_refs.playerLifeMax=offsetof(Player,lifeMax);
    Player livePlayer;
    accessory.prefix=98;
    const int beforeAccessory=prefixWorkerCalls;
    hookGrant(&livePlayer.object,&accessory.object,nullptr);
    assert(livePlayer.defense==16 && prefixWorkerCalls==beforeAccessory+1);
    livePlayer.defense=10;
    hookGrant(&livePlayer.object,&accessory.object,nullptr);
    assert(livePlayer.defense==16 && prefixWorkerCalls==beforeAccessory+1);
    livePlayer.life=200;
    livePlayer.defense=10;
    hookGrant(&livePlayer.object,&accessory.object,nullptr);
    assert(livePlayer.defense==14 && prefixWorkerCalls==beforeAccessory+1);
    std::this_thread::sleep_for(std::chrono::milliseconds(270));
    livePlayer.defense=10;
    hookGrant(&livePlayer.object,&accessory.object,nullptr);
    assert(livePlayer.defense==13 && prefixWorkerCalls==beforeAccessory+2);
    prefixWorkerFails=true;
    std::this_thread::sleep_for(std::chrono::milliseconds(270));
    livePlayer.defense=10;
    hookGrant(&livePlayer.object,&accessory.object,nullptr);
    assert(livePlayer.defense==14 && prefixWorkerCalls==beforeAccessory+3);
    prefixWorkerFails=false;
    g_refs.playerLife=-1;
    livePlayer.defense=10;
    hookGrant(&livePlayer.object,&accessory.object,nullptr);
    assert(livePlayer.defense==14 && prefixWorkerCalls==beforeAccessory+3);
    prefixAssemblies=std::make_shared<const std::map<std::string,std::string>>();
    prefixAssemblies=std::make_shared<const std::map<std::string,std::string>>(
        std::map<std::string,std::string>{{"test","/tmp/prefix-fixture.dll"}});
    prefixWorkerWarmup=1;
    prefixWorkerFails=false;
    livePrefix.callbackMask=3;
    weapon.prefix=0;
    weapon.damage=12;
    weapon.mana=0;
    weapon.stack=1;
    weapon.ranged=1;
    weapon.accessory=0;
    weapon.consumable=0;
    {
        const int beforeItemChange=prefixWorkerCalls;
        PrefixRpcScope scope;
        assert(itemRollWeight(&weapon.object,livePrefix)==7);
        assert(itemRollWeight(&weapon.object,livePrefix)==7);
        assert(prefixWorkerCalls==beforeItemChange+1);
        weapon.damage=13;
        assert(itemRollWeight(&weapon.object,livePrefix)==7);
        assert(prefixWorkerCalls==beforeItemChange+2);
    }
    weapon.damage=12;
    prefixAssemblies=std::make_shared<const std::map<std::string,std::string>>();
    itemCallbackView=std::make_shared<const std::map<int,ManagedItemPrefixCallbackBinding>>(
        std::map<int,ManagedItemPrefixCallbackBinding>{{6199,{"test","TestWand",15}}});
    itemCallbackAssemblies=std::make_shared<const std::map<std::string,std::string>>(
        std::map<std::string,std::string>{{"test","/tmp/prefix-fixture.dll"}});
    itemWorkerWarmup=1;
    itemWorkerCalls=0;
    itemWorkerDenied=99;
    itemDecisionCalls=0;
    itemChoice=73;
    itemChance=1;
    itemPostDamage=3;
    itemPostValue=15;
    {
        PrefixRpcScope scope;
        std::string output;
        assert(queryItemPrefixDecision(&weapon.object,0,3,&output) && output=="73");
        assert(queryItemPrefixDecision(&weapon.object,1,4,&output) && output=="1");
        assert(queryItemPrefixDecision(&weapon.object,72,5,&output) && output=="3,15");
        assert(itemDecisionCalls==3);
    }
    itemChoice=-1;
    itemChance=-1;
    itemPostDamage=0;
    itemPostValue=0;
    weapon.type=6199;
    weapon.damage=12;
    weapon.mana=0;
    weapon.stack=1;
    weapon.prefix=0;
    weapon.accessory=0;
    weapon.ranged=1;
    weapon.consumable=0;
    {
        PrefixRpcScope scope;
        assert(!itemAllowsPrefix(&weapon.object,99));
        assert(!itemAllowsPrefix(&weapon.object,99));
        assert(itemWorkerCalls==1);
        weapon.damage=13;
        assert(!itemAllowsPrefix(&weapon.object,99));
        assert(itemWorkerCalls==2);
        assert(itemAllowsPrefix(&weapon.object,98));
        assert(itemWorkerCalls==3);
    }
    weapon.damage=12;
    const int nativeRollsBeforeItemWorker=nativeRolls;
    assert(hookRollable(&weapon.object,nullptr)==nullptr);
    int32_t deniedRoll=-1;
    assert(!hookRoll(&weapon.object,&randomObject,&deniedRoll,nullptr));
    assert(nativeRolls==nativeRollsBeforeItemWorker);
    const int beforeDeniedItemApply=chanceOriginalCalls;
    assert(!hookItemPrefixChance(&weapon.object,99,&chanceTier,nullptr));
    assert(chanceOriginalCalls==beforeDeniedItemApply);
    g_restoreRoll={&weapon.object,99,false};
    assert(hookItemPrefixChance(&weapon.object,99,&chanceTier,nullptr));
    assert(chanceOriginalCalls==beforeDeniedItemApply+1);
    g_restoreRoll={};
    weapon.prefix=0;
    itemWorkerFails=true;
    assert(itemAllowsPrefix(&weapon.object,99));
    assert(hookRollable(&weapon.object,nullptr)!=nullptr);
    itemWorkerFails=false;
    itemWorkerWarmup=0;
    assert(itemAllowsPrefix(&weapon.object,99));
    assert(hookRollable(&weapon.object,nullptr)!=nullptr);
    itemWorkerWarmup=1;
    itemWorkerDenied=71;
    {
        PrefixRpcScope scope;
        for (int index=70;index<=73;++index) assert(itemAllowsPrefix(&weapon.object,index)==(index!=71));
        const int count=itemWorkerCalls;
        assert(itemAllowsPrefix(&weapon.object,74));
        assert(itemWorkerCalls==count);
    }
    itemCallbackView=std::make_shared<const std::map<int,ManagedItemPrefixCallbackBinding>>(
        std::map<int,ManagedItemPrefixCallbackBinding>{{6196,{"test","TestCharm"}}});
    descriptorView=std::make_shared<const std::vector<ManagedPrefixEntry>>();
    Item nativeItem;
    nativeItem.type=6196;
    nativeItem.accessory=1;
    nativeItem.damage=-1;
    nativeItem.prefix=0;
    itemWorkerDenied=99;
    const int beforeNativeUnfiltered=nativeRolls;
    int32_t vanillaPrefix=-1;
    assert(hookRoll(&nativeItem.object,&randomObject,&vanillaPrefix,nullptr));
    assert(vanillaPrefix==71 && nativeRolls==beforeNativeUnfiltered+1);
    itemWorkerDenied=71;
    assert(hookRollable(&nativeItem.object,nullptr)==nullptr);
    assert(!hookRoll(&nativeItem.object,&randomObject,&vanillaPrefix,nullptr));
    assert(nativeRolls==beforeNativeUnfiltered+1);
    itemWorkerFails=true;
    assert(hookRoll(&nativeItem.object,&randomObject,&vanillaPrefix,nullptr));
    assert(vanillaPrefix==71 && nativeRolls==beforeNativeUnfiltered+2);
    itemWorkerFails=false;
    batchVanillaCandidates={71,72,73,74,75,76,77};
    itemWorkerDenied=75;
    const int callsBeforeBatch=itemWorkerCalls;
    auto* batched=hookRollable(&nativeItem.object,nullptr);
    assert(batched && batched->length==6);
    assert(itemWorkerCalls==callsBeforeBatch+1);
    const auto* batchIds=static_cast<int32_t*>(arrayData(batched));
    for (uintptr_t i=0;i<batched->length;++i)
        assert(batchIds[i]!=75);
    batchVanillaCandidates.clear();
    itemCallbackView=std::make_shared<const std::map<int,ManagedItemPrefixCallbackBinding>>();
    itemCallbackAssemblies=std::make_shared<const std::map<std::string,std::string>>();
    globalView=std::make_shared<const std::vector<ManagedGlobalPrefixCallbackBinding>>();
    const auto prefixGroup=allocate(4,sizeof(int32_t));
    auto* categoryIds=static_cast<int32_t*>(arrayData(prefixGroup));
    categoryIds[0]=71;
    categoryIds[1]=72;
    categoryIds[2]=72;
    categoryIds[3]=73;
    g_refs.vanillaPrefixGroups[4]=prefixGroup;
    auto* vanillaBase=allocate(2,sizeof(int32_t));
    auto* baseIds=static_cast<int32_t*>(arrayData(vanillaBase));
    baseIds[0]=71;
    baseIds[1]=74;
    Item categorizedItem;
    categorizedItem.type=6199;
    categorizedItem.damage=20;
    categorizedItem.ammo=0;
    categorizedItem.consumable=0;
    auto inherited=vanillaItemCandidates(&categorizedItem.object,vanillaBase);
    assert((inherited==std::vector<int32_t>{71,74,72,73}));
    categorizedItem.consumable=1;
    assert(vanillaItemCandidates(&categorizedItem.object,vanillaBase).size()==2);
    categorizedItem.consumable=0;
    categorizedItem.type=100;
    assert(vanillaItemCandidates(&categorizedItem.object,vanillaBase).size()==2);
    g_refs.vanillaPrefixGroups[4]=nullptr;
    const auto metrics = managedPrefixDiagnostics();
    assert(metrics.rollAttempts >= 12);
    assert(metrics.restoredRolls >= 1);
    assert(metrics.chosenItemRolls >= 2);
    assert(metrics.weightedRolls >= 3);
    assert(metrics.vanillaFallbacks >= 1);
    assert(metrics.rollAttempts >=
           metrics.restoredRolls + metrics.chosenItemRolls +
           metrics.preferredRolls + metrics.weightedRolls);
    assert(metrics.totalRollMicroseconds >= metrics.maxRollMicroseconds);
    for(void* p : allocations) std::free(p);
    std::cout << "cold tables, native prefix diagnostics, save restoration and tooltips: PASS\n";
}
