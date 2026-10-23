#define main existingNativeItemFixtureMain
#include "native_item_binding_test.cpp"
#undef main
#include <limits>

int main(int argc,char** argv) {
    using namespace bl::csharp;
    assert(argc>=2);configure();
    const bool sample=argc>2&&std::string(argv[2])=="--sample";
    std::string error,line;std::ifstream wire(argv[1]);assert(wire);std::getline(wire,line);
    std::vector<BasicItemEntry> entries;int next=6400;std::string mixedBehavior;
    while(std::getline(wire,line)) {
        auto fields=basicSplit(line,'\t');if(fields[0]!="ITEM")continue;
        BasicItemEntry entry;assert(parseBasicItemWire(fields,&entry,&error));
        if(entry.accessoryEffects.empty())continue;
        if(sample||entry.name=="ExampleStatAccessory")assert(entry.behavior.empty());
        else {
            assert(entry.name=="WaspNest"&&entry.behavior.find("Back")!=std::string::npos);
            assert(entry.behavior.find("\"nativeAccessory\":true")!=std::string::npos);
            // Isolate StrongBees from the visual policy binder in this fixture.
            // Native Back rendering is covered by native_item_policies_test.cpp.
            mixedBehavior=entry.behavior;entry.behavior.clear();
        }
        entry.type=next++;identities[entry.type]="mod/"+entry.name;
        assert(registerBasicItemPlaceholder(entry.type,entry.name,&error));entries.push_back(entry);
    }
    assert(entries.size()==(sample?1u:2u));
    if(sample) {
        const auto& entry=entries[0];assert(entry.name=="NativeCharm"&&entry.defense==2);
        assert(applyNativeBasicItemDefaults("mod",entries,&error));assert(functionalHook&&hookInstalls==1);
        FakePlayer player;FakeItem item{};item.type=entry.type;item.flags[5]=true;
        functionalHook(&player.header,3,&item.header,&functionalMethod);
        assert(std::abs(player.meleeDamage-2.2f)<0.00001f&&std::abs(player.moveSpeed-1.05f)<0.00001f&&player.strongBees);
        assert(pendingBasicItemBehaviors().empty()&&basicItemPolicyReady(entry.type));
        player=FakePlayer{};item.flags[5]=false;functionalHook(&player.header,3,&item.header,&functionalMethod);
        assert(player.meleeDamage==2&&player.moveSpeed==1&&!player.strongBees);
        std::cout<<"NATIVE_CHARM_INDEPENDENT_SAMPLE_WITHOUT_QUICKJS_PASS\n";return 0;
    }
    if(argc>2&&std::string(argv[2])=="--fallback") {
        auto mixed=entries[1];for(const auto& entry:entries)if(entry.name=="WaspNest")mixed=entry;
        mixed.behavior=mixedBehavior;assert(!mixed.behavior.empty());
        bl::il2cpp::findField(&playerClass,"strongBees")->type=&intType;
        assert(!applyNativeBasicItemDefaults("mod",{mixed},&error));
        assert(error.find("NativeTypeMismatch")!=std::string::npos);
        assert(pendingBasicItemBehaviors().empty());
        std::cout<<"NATIVE_MIXED_ACCESSORY_REJECTS_INVALID_METADATA_WITHOUT_JS_FALLBACK_PASS\n";return 0;
    }
    if(argc>2) {
        auto* f=bl::il2cpp::findField(&playerClass,"meleeDamage");
        const std::string mode=argv[2];
        if(mode=="--type")f->type=&intType;
        else if(mode=="--static")f->flags=0x10;
        else if(mode=="--bounds")f->offset=sizeof(FakePlayer);
        else if(mode=="--hook")rejectEquipHook=true;
        else assert(false);
        assert(!applyNativeBasicItemDefaults("mod",entries,&error));
        assert(error.find(mode=="--hook"?"FunctionalHookUnavailable":mode=="--bounds"?"InvalidFieldBounds":"NativeTypeMismatch")!=std::string::npos);
        assert(pendingBasicItemBehaviors().empty());
        std::cout<<"NATIVE_ACCESSORY_CONTRACT_REJECTION_PASS "<<mode<<"\n";return 0;
    }
    assert(applyNativeBasicItemDefaults("mod",entries,&error));assert(functionalHook&&hookInstalls==1);
    auto find=[&](const std::string& name)->const BasicItemEntry& {
        for(const auto& entry:entries)if(entry.name==name)return entry;
        assert(false);return entries[0];
    };
    const auto& stat=find("ExampleStatAccessory");const auto& wasp=find("WaspNest");
    FakePlayer player;FakeItem item{};item.type=stat.type;item.flags[5]=true;
    functionalHook(&player.header,3,&item.header,&functionalMethod);
    assert(originalEquipCalls==1);
    assert(std::abs(player.meleeDamage-2.2f)<0.00001f&&std::abs(player.rangedDamage-3.3f)<0.00001f&&
           std::abs(player.magicDamage-4.4f)<0.00001f&&player.minionDamage==5&&player.moveSpeed==1&&!player.strongBees);
    item.type=wasp.type;functionalHook(&player.header,4,&item.header,&functionalMethod);assert(player.strongBees);
    for(auto sentinel:player.sentinel)assert(sentinel==0x5a);
    player=FakePlayer{};item.flags[5]=false;functionalHook(&player.header,3,&item.header,&functionalMethod);assert(!player.strongBees);
    item.type=999;item.flags[5]=true;functionalHook(&player.header,3,&item.header,&functionalMethod);assert(player.meleeDamage==2&&!player.strongBees);
    // Re-equipping after Terraria resets effects must not retain prior player/loadout state.
    for(int i=0;i<10000;++i) {
        player=FakePlayer{};item.type=stat.type;functionalHook(&player.header,3,&item.header,&functionalMethod);
        assert(std::abs(player.meleeDamage-2.2f)<0.00001f);
    }
    assert(hookInstalls==1&&pendingBasicItemBehaviors().empty()&&basicItemPolicyReady(stat.type));
    // A non-finite native input rejects the complete policy rather than poisoning more stats.
    player=FakePlayer{};player.meleeDamage=std::numeric_limits<float>::infinity();
    functionalHook(&player.header,3,&item.header,&functionalMethod);assert(player.rangedDamage==3&&player.magicDamage==4);
    BasicItemEntry parseItem;parseItem.values[BasicFlags]=32;
    auto parse=[&](const std::string& value){parseItem.accessoryEffects.clear();return parseBasicAccessoryEffects(value,&parseItem,&error);};
    for(const auto& invalid:{"","meleeDamage:bad:1","other:add:1","strongBees:add:1","strongBees:set:0",
                             "meleeDamage:mul:-1","meleeDamage:add:11","meleeDamage:add:nan","meleeDamage:add:inf",
                             "meleeDamage:add:1;","meleeDamage:add:1junk","meleeDamage:add: 1"})assert(!parse(invalid));
    auto many=std::string("meleeDamage:add:1");for(int i=0;i<16;++i)many+=";meleeDamage:add:1";assert(!parse(many));
    assert(parse("meleeDamage:add:-0.1;meleeDamage:mul:2;moveSpeed:add:0.5;strongBees:set:1"));
    auto ordered=stat;ordered.type=6500;ordered.name="Ordered";ordered.accessoryEffects=parseItem.accessoryEffects;
    identities[ordered.type]="mod/Ordered";assert(registerBasicItemPlaceholder(ordered.type,ordered.name,&error));
    assert(applyNativeBasicItemDefaults("mod",{ordered},&error));
    player=FakePlayer{};item.type=ordered.type;functionalHook(&player.header,3,&item.header,&functionalMethod);
    assert(std::abs(player.meleeDamage-3.8f)<0.00001f&&player.moveSpeed==1.5f&&player.strongBees);
    std::cout<<"NATIVE_ACCESSORY_REAL_ROSLYN_WIRE_WITHOUT_QUICKJS_PASS\n";
}
