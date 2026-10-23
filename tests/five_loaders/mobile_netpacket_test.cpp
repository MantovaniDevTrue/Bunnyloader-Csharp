// Execute the actual production sender with the Mobile value-type layout.
// A boxed header passed as NetPacket.this or as a by-value argument fails.
#include "il2cpp/Api.h"
struct Il2CppClass {};
struct MethodInfo {};
#include "experiments/csharp/NativeWorldNetTransport.cpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <array>
#include <cstdlib>
#include <set>
#include <iostream>
using namespace bl::csharp::worldsave;
namespace {
bl::il2cpp::Api fixture;
Il2CppClass packetType, bytesType;
MethodInfo ctorMethod, writerMethod, shrinkMethod, sendMethod, broadcastMethod,
           managerMethod, writeMethod;
struct PacketBox { Il2CppObject object; std::array<unsigned char, 16> value; } box;
Il2CppObject managerObject{}, writerObject{};
std::vector<unsigned char> allocation;
std::vector<uint8_t> fixturePacket;
int calls = 0, directed = 0, broadcasts = 0;
std::set<uint32_t> roots;
uint32_t nextRoot = 0;
bool valueType = true, failWriter = false;
Il2CppObject* fakeInvoke(const MethodInfo* method, void* self, void** args, Il2CppObject** error) {
    *error = nullptr;
    ++calls;
    if (method == &managerMethod) { assert(!self); return &managerObject; }
    if (method == &ctorMethod) {
        assert(self == box.value.data());
        assert(*static_cast<uint16_t*>(args[0]) == kWorldNetworkModuleId);
        assert(*static_cast<int*>(args[1]) >= static_cast<int>(fixturePacket.size()));
        std::memcpy(self, args[0], 2);
    } else if (method == &writerMethod) {
        assert(self == box.value.data());
        if (failWriter) { *error = &writerObject; return nullptr; }
        return &writerObject;
    } else if (method == &writeMethod) {
        assert(self == &writerObject);
        auto* bytes = static_cast<Il2CppArray*>(args[0]);
        assert(bytes->length == fixturePacket.size());
        assert(std::memcmp(arrayData(bytes), fixturePacket.data(), fixturePacket.size()) == 0);
    } else if (method == &shrinkMethod) {
        assert(self == box.value.data());
    } else if (method == &sendMethod || method == &broadcastMethod) {
        assert(self == &managerObject && args[0] == box.value.data());
        uint16_t module = 0; std::memcpy(&module, args[0], 2);
        assert(module == kWorldNetworkModuleId);
        if (method == &sendMethod) { assert(*static_cast<int*>(args[1]) == 7); ++directed; }
        else { assert(*static_cast<int*>(args[1]) == 3); ++broadcasts; }
    } else assert(false);
    return nullptr;
}
}
namespace bl::il2cpp { Api& api() { return fixture; } }
int main(int argc, char** argv) {
    assert(argc == 2);
    std::ifstream input(argv[1], std::ios::binary);
    fixturePacket.assign(std::istreambuf_iterator<char>(input), {});
    assert(validWorldNetworkPacket(fixturePacket.data(), fixturePacket.size()));
    box.object.klass = &packetType;
    box.object.monitor = reinterpret_cast<void*>(0x1234);
    g_packetClass = &packetType; g_byteClass = &bytesType;
    g_packetCtor = &ctorMethod; g_getWriter = &writerMethod;
    g_shrink = &shrinkMethod; g_sendClient = &sendMethod;
    g_broadcast = &broadcastMethod; g_getInstance = &managerMethod;
    g_writeBytes = &writeMethod;
    g_modeField = reinterpret_cast<FieldInfo*>(1);
    fixture.field_static_get_value = [](FieldInfo*, void* p) { *static_cast<int*>(p) = 2; };
    fixture.class_is_valuetype = [](const Il2CppClass* cls) { return valueType && cls == &packetType; };
    fixture.gchandle_new = [](Il2CppObject* object, bool pinned) -> uint32_t {
        assert(object && pinned); roots.insert(++nextRoot); return nextRoot;
    };
    fixture.gchandle_free = [](uint32_t id) { assert(roots.erase(id) == 1); };
    fixture.object_new = [](Il2CppClass* cls) { assert(cls == &packetType); return &box.object; };
    fixture.array_new = [](Il2CppClass*, uintptr_t length) {
        allocation.assign(sizeof(Il2CppArray) + length, 0);
        auto* array = reinterpret_cast<Il2CppArray*>(allocation.data());
        array->length = length; return array;
    };
    fixture.runtime_invoke = fakeInvoke;
    std::string error;
    assert(sendNativeWorldNetPacket(fixturePacket, 7, -1, &error));
    assert(directed == 1 && roots.empty());
    assert(sendNativeWorldNetPacket(fixturePacket, -1, 3, &error));
    assert(broadcasts == 1 && roots.empty());
    assert(box.object.klass == &packetType && box.object.monitor == reinterpret_cast<void*>(0x1234));
    failWriter = true;
    assert(!sendNativeWorldNetPacket(fixturePacket, 7, -1, &error));
    assert(directed == 1 && roots.empty());
    failWriter = false; valueType = false;
    const int previousCalls = calls;
    assert(!sendNativeWorldNetPacket(fixturePacket, 7, -1, &error));
    assert(calls == previousCalls && roots.empty());
    std::cout << "MOBILE_NETPACKET_UNBOXED_GC_AND_FAILURE_PATHS_PASS\n";
}
