#include "graphics/ScreenSceneFieldContract.h"
#include <cassert>
#include <cstdlib>
#include <vector>

namespace {
struct Class;
struct Type { const char* name; Class* klass; };
struct Field { const char* name; Type* type; uint32_t flags; size_t offset; void* value; };
struct Class { int size; bool value; std::vector<Field*> fields; };
int copies = 0;
template<class T, class U> T* as(U* p) { return reinterpret_cast<T*>(p); }
}
namespace bl::il2cpp { Api& api() { static Api instance; return instance; } }

int main() {
    auto& api = bl::il2cpp::api();
    api.type_get_name = [](const Il2CppType* type) { return strdup(as<const Type>(type)->name); };
    api.il2cpp_free = std::free;
    api.class_from_il2cpp_type = [](const Il2CppType* type) { return as<Il2CppClass>(as<const Type>(type)->klass); };
    api.class_is_valuetype = [](const Il2CppClass* cls) { return as<const Class>(cls)->value; };
    api.class_value_size = [](Il2CppClass* cls, uint32_t* align) { *align = 4; return as<Class>(cls)->size; };
    api.class_get_fields = [](Il2CppClass* cls, void** iterator) -> FieldInfo* {
        auto& fields = as<Class>(cls)->fields;
        const size_t index = reinterpret_cast<size_t>(*iterator);
        if (index >= fields.size()) return nullptr;
        *iterator = reinterpret_cast<void*>(index + 1);
        return as<FieldInfo>(fields[index]);
    };
    api.field_get_type = [](FieldInfo* f) { return as<const Il2CppType>(as<Field>(f)->type); };
    api.field_get_flags = [](FieldInfo* f) { return as<Field>(f)->flags; };
    api.field_get_offset = [](FieldInfo* f) { return as<Field>(f)->offset; };
    api.class_get_field_from_name = [](Il2CppClass* cls, const char* name) -> FieldInfo* {
        for (auto* f : as<Class>(cls)->fields) if (!strcmp(f->name, name)) return as<FieldInfo>(f);
        return nullptr;
    };
    api.field_static_get_value = [](FieldInfo* f, void* out) {
        ++copies; auto* field = as<Field>(f);
        memcpy(out, field->value, field->type->klass->size);
    };
    Class boolean{1, true, {}}, integer{4, true, {}}, real{4, true, {}}, wide{8, true, {}};
    Type boolType{"System.Boolean", &boolean}, intType{"System.Int32", &integer},
         floatType{"System.Single", &real}, doubleType{"System.Double", &wide};
    int width = 1280; Field field{"screenWidth", &intType, 0x10, 4, &width};
    using namespace bl::graphics;
    auto* bound = bindScreenSceneField(as<FieldInfo>(&field), "System.Int32", 4, "Main.screenWidth");
    assert(bound);
    struct { int value; uint32_t canary; } output{0, 0xabcdef12};
    api.field_static_get_value(bound, &output.value);
    assert(output.value == 1280 && output.canary == 0xabcdef12 && copies == 1);
    field.type = &doubleType;
    assert(!bindScreenSceneField(as<FieldInfo>(&field), "System.Int32", 4, "Main.screenWidth"));
    assert(screenSceneTestLog.find("Expected=System.Int32:4 Actual=System.Double:8") != std::string::npos);
    field.type = &intType; integer.size = 8;
    assert(!bindScreenSceneField(as<FieldInfo>(&field), "System.Int32", 4, "Main.screenWidth"));
    integer.size = 4; field.flags = 0;
    assert(!bindScreenSceneField(as<FieldInfo>(&field), "System.Int32", 4, "Main.screenWidth"));
    field.flags = 0x10; field.offset = size_t(-1);
    assert(!bindScreenSceneField(as<FieldInfo>(&field), "System.Int32", 4, "Main.screenWidth"));
    assert(copies == 1 && output.canary == 0xabcdef12);
    field.offset = 0; field.type = &boolType;
    assert(bindScreenSceneField(as<FieldInfo>(&field), "System.Boolean", 1, "Main.dayTime"));
    Field x{"X", &floatType, 0, sizeof(Il2CppObject), nullptr};
    Field y{"Y", &floatType, 0, sizeof(Il2CppObject) + 4, nullptr};
    Class vector{8, true, {&x, &y}};
    Type vectorType{"Microsoft.Xna.Framework.Vector2", &vector};
    field.type = &vectorType;
    assert(bindScreenSceneVectorField(as<FieldInfo>(&field), "Main.screenPosition"));
    vectorType.name = "UnityEngine.Vector2"; x.name = "x"; y.name = "y";
    assert(bindScreenSceneVectorField(as<FieldInfo>(&field), "Main.screenPosition"));
    vector.size = 16;
    assert(!bindScreenSceneVectorField(as<FieldInfo>(&field), "Main.screenPosition"));
    vector.size = 8; std::swap(x.offset, y.offset);
    assert(!bindScreenSceneVectorField(as<FieldInfo>(&field), "Main.screenPosition"));
    std::swap(x.offset, y.offset); x.type = &doubleType;
    assert(!bindScreenSceneVectorField(as<FieldInfo>(&field), "Main.screenPosition"));
    assert(copies == 1);
    puts("SCREEN_SCENE_NATIVE_TYPE_SIZE_STORAGE_VECTOR_AND_CANARY=PASS");
}
