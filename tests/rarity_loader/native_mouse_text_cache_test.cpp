// Exercita o layout e o escopo reais do renderer, sem JS nem APK.
#include "experiments/csharp/NativeRarityGameplay.cpp"
#include <cassert>
#include <iostream>

struct Il2CppClass { bool value; int size; };
struct Il2CppType { Il2CppClass* cls; };
struct FieldInfo { const char* name; Il2CppType type; size_t offset; uint32_t flags = 0; };
bl::il2cpp::Api fixture;
Il2CppClass cache{true,48}, integer{true,4}, byte{true,1}, boolean{true,1}, stringType{false,0}, item{false,64};
std::vector<FieldInfo> fields;
FieldInfo hoverField{"HoverItem",{&item},0,0x10};
Il2CppObject* hovered = nullptr;
Il2CppObject* expectedPayload = nullptr;
Il2CppObject* expectedSelf = nullptr;
int forwards = 0;
void originalMouse(Il2CppObject* self, Il2CppObject* payload, bool world, const MethodInfo* method) {
    assert(self==expectedSelf && payload==expectedPayload && world && !method);
    assert(bl::csharp::g_rarityTextTarget);
    ++forwards;
}
void originalTooltip(Il2CppObject* self, Il2CppObject* payload, int32_t rare,
                     uint8_t diff, int32_t x, int32_t y, const MethodInfo* method) {
    assert(self==expectedSelf && payload==expectedPayload && rare==12 && diff==0 && x==71 && y==83 && !method);
    ++forwards;
}
namespace bl::il2cpp {
Api& api() { return fixture; }
FieldInfo* findField(Il2CppClass* cls, std::string_view name) {
    if (cls != &cache) return nullptr;
    for (auto& f : fields) if (name == f.name) return &f;
    return nullptr;
}
}
void setup(size_t header = 0) {
    cache = {true,48};
    fields = {{"noOverride",{&boolean},0},{"isValid",{&boolean},1},
        {"cursorText",{&stringType},8},{"rare",{&integer},16},
        {"diff",{&byte},20},{"X",{&integer},24},{"Y",{&integer},28},
        {"hackedScreenWidth",{&integer},32},{"hackedScreenHeight",{&integer},36},
        {"buffTooltip",{&stringType},40}};
    for (auto& f : fields) f.offset += header;
}
int main() {
    fixture.class_is_valuetype = [](const Il2CppClass* c) {return c->value;};
    fixture.class_value_size = [](Il2CppClass* c, uint32_t* a) { *a=8; return c->size; };
    fixture.class_get_fields = [](Il2CppClass* c, void** it) -> FieldInfo* {
        auto i = reinterpret_cast<size_t>(*it);
        if (c != &cache || i >= fields.size()) return nullptr;
        *it = reinterpret_cast<void*>(i+1); return &fields[i];
    };
    fixture.field_get_type = [](FieldInfo* f) -> const Il2CppType* { return &f->type; };
    fixture.field_get_offset = [](FieldInfo* f) {return f->offset;};
    fixture.field_get_flags = [](FieldInfo* f) {return f->flags;};
    fixture.class_from_il2cpp_type = [](const Il2CppType* t) {return t->cls;};
    fixture.field_static_get_value = [](FieldInfo* f, void* out) {
        assert(f==&hoverField); std::memcpy(out, &hovered, sizeof(hovered));
    };
    using namespace bl::csharp;
    std::string error;
    NativeRarityMouseTextCache layout;
    auto bind = [&](bool arm64=true) {return layout.bind(&cache,&integer,&byte,&boolean,&stringType,&error,arm64);};
    for (size_t header : {size_t{0}, sizeof(Il2CppObject)}) {
        setup(header); assert(bind());
        assert(layout.rare==16 && layout.text==8 && layout.buff==40 && layout.diff==20 && layout.diffWidth==1);
        // Payload de 48 bytes, sem cabeçalho. Os acessos abaixo são os mesmos
        // executados pelo hook de produção após a passagem indireta no ARM64.
        alignas(8) std::array<std::byte,48> payload{};
        Il2CppString text{}; text.length=3;
        auto* textPtr=&text;
        int32_t rarity=12;
        std::memcpy(payload.data()+8,&textPtr,sizeof(textPtr));
        std::memcpy(payload.data()+16,&rarity,4);
        g_cacheRareOffset=layout.rare; g_cacheTextOffset=layout.text;
        g_cacheBuffOffset=layout.buff; g_cacheDiffOffset=layout.diff; g_cacheDiffWidth=layout.diffWidth;
        g_groundTextHookReady.store(true);
        g_hoverItem={}; g_hoverItem.field=&hoverField; g_hoverItem.type=&item;
        assert(g_palette.publish(12,0xff123456));
        auto* info=reinterpret_cast<Il2CppObject*>(payload.data());
        int calls=0;
        rarityMouseScope(info,[&] { ++calls; assert(g_rarityTextTarget==&text && g_rarityTextColor==0xff123456); });
        assert(calls==1 && g_rarityTextTarget==nullptr);
        Il2CppObject self{};
        expectedSelf=&self; expectedPayload=info;
        g_originalRarityMouseInstance=originalMouse;
        rarityMouseInstance(&self,info,true,nullptr);
        assert(g_rarityTextTarget==nullptr);
        g_originalRarityTooltipInstance=originalTooltip;
        rarityTooltipInstance(&self,info,12,0,71,83,nullptr);
        assert(forwards==static_cast<int>(header ? 4 : 2));
        // Escopos aninhados devem restaurar o alvo e a cor anteriores.
        g_rarityTextTarget=&text; g_rarityTextColor=0xffabcdef;
        g_rarityTextMatchByContent=true;
        rarityMouseScope(info,[&] { assert(g_rarityTextColor==0xff123456 && !g_rarityTextMatchByContent); });
        assert(g_rarityTextTarget==&text && g_rarityTextColor==0xffabcdef && g_rarityTextMatchByContent);
        g_rarityTextTarget=nullptr; g_rarityTextMatchByContent=false;
        // Expert/master text and buff text must retain the original color.
        payload[20]=std::byte{1};
        rarityMouseScope(info,[&] { ++calls; assert(g_rarityTextTarget==nullptr); });
        payload[20]=std::byte{0};
        std::memcpy(payload.data()+40,&textPtr,sizeof(textPtr));
        rarityMouseScope(info,[&] { ++calls; assert(g_rarityTextTarget==nullptr); });
        assert(calls==3);
        assert(!bind(false) && layout.rare==-1);
    }
    setup(); cache.size=40; assert(!bind());
    setup(); fields[0].offset=2; assert(!bind());
    setup(); fields[4].type.cls=&integer; assert(!bind());
    setup(); fields[2].flags=0x10; assert(!bind());
    setup(); fields[9].offset=48; assert(!bind());
    setup(); fields[3].type.cls=&byte; assert(!bind());
    setup(); fields.erase(fields.begin()+1); assert(!bind());
    setup(sizeof(Il2CppObject)); cache.value=false;
    assert(bind(false) && layout.rare==32 && layout.diff==36);
    fields[4].type.cls=&integer;
    assert(bind(false) && layout.diffWidth==4);
    fields[2].offset=0; assert(!bind(false));
    std::cout << "MouseTextCache: raw/boxed metadata, payload reads, scoped colors, legacy class and rejected ABIs PASS\n";
}
