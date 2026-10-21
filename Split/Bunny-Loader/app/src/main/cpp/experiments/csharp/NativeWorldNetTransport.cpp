#include "experiments/csharp/NativeWorldNetTransport.h"
#include "experiments/csharp/NativeWorldNetwork.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace bl::csharp::worldsave {
namespace {
using ReadFn = void (*)(Il2CppObject*, Il2CppObject*, int, int, const MethodInfo*);
using SendWorldFn = void (*)(int, int, int, Il2CppObject*, int, float, float,
                             float, int, int, int, const MethodInfo*);
ReadFn g_originalRead = nullptr;
SendWorldFn g_originalSendData = nullptr;
const MethodInfo* g_readUInt16 = nullptr;
const MethodInfo* g_readBytes = nullptr;
const MethodInfo* g_getBaseStream = nullptr;
const MethodInfo* g_getPosition = nullptr;
const MethodInfo* g_setPosition = nullptr;
const MethodInfo* g_packetCtor = nullptr;
const MethodInfo* g_getWriter = nullptr;
int32_t g_writerOffset = -1;
const MethodInfo* g_writeBytes = nullptr;
const MethodInfo* g_shrink = nullptr;
const MethodInfo* g_getInstance = nullptr;
const MethodInfo* g_sendClient = nullptr;
const MethodInfo* g_broadcast = nullptr;
FieldInfo* g_instanceField = nullptr;
FieldInfo* g_modeField = nullptr;
Il2CppClass* g_packetClass = nullptr;
Il2CppClass* g_byteClass = nullptr;

int netMode() {
    if (!g_modeField) return -1;
    int mode = -1;
    il2cpp::api().field_static_get_value(g_modeField, &mode);
    return mode;
}

Il2CppObject* invoke(const MethodInfo* method, Il2CppObject* self,
                     void** args, bool* good) {
    if (good) *good = false;
    if (!method) return nullptr;
    Il2CppObject* exception = nullptr;
    Il2CppObject* value = il2cpp::api().runtime_invoke(
        method, self, args, &exception);
    if (good) *good = exception == nullptr;
    return exception ? nullptr : value;
}

bool readPosition(Il2CppObject* stream, int64_t* position) {
    bool ok = false;
    Il2CppObject* boxed = invoke(g_getPosition, stream, nullptr, &ok);
    if (!ok || !boxed || !position) return false;
    std::memcpy(position, reinterpret_cast<char*>(boxed) + sizeof(Il2CppObject),
                sizeof(*position));
    return true;
}

void rewind(Il2CppObject* stream, int64_t position) {
    if (!stream || !g_setPosition) return;
    void* args[] = {&position};
    invoke(g_setPosition, stream, args, nullptr);
}

Il2CppObject* manager() {
    if (g_getInstance) {
        bool ok = false;
        Il2CppObject* value = invoke(g_getInstance, nullptr, nullptr, &ok);
        if (ok) return value;
    }
    Il2CppObject* result = nullptr;
    if (g_instanceField)
        il2cpp::api().field_static_get_value(g_instanceField, &result);
    return result;
}

void hkWorldNetRead(Il2CppObject* self, Il2CppObject* reader, int userId,
                    int readLength, const MethodInfo* method) {
    if (!g_originalRead) return;
    if (!bl::csharp::nativeWorldNetActive() || !reader || readLength < 2) {
        g_originalRead(self, reader, userId, readLength, method);
        return;
    }
    bool ok = false;
    Il2CppObject* stream = invoke(g_getBaseStream, reader, nullptr, &ok);
    int64_t start = 0;
    if (!ok || !stream || !readPosition(stream, &start)) {
        g_originalRead(self, reader, userId, readLength, method);
        return;
    }
    Il2CppObject* idValue = invoke(g_readUInt16, reader, nullptr, &ok);
    if (!ok || !idValue) {
        rewind(stream, start);
        g_originalRead(self, reader, userId, readLength, method);
        return;
    }
    uint16_t moduleId = 0;
    std::memcpy(&moduleId, reinterpret_cast<char*>(idValue) + sizeof(Il2CppObject),
                sizeof(moduleId));
    if (moduleId != kWorldNetworkModuleId) {
        rewind(stream, start);
        g_originalRead(self, reader, userId, readLength, method);
        return;
    }
    if (netMode() != 1 || readLength - 2 > static_cast<int>(kWorldNetworkMaxBytes))
        return;
    int remaining = readLength - 2;
    void* args[] = {&remaining};
    Il2CppObject* result = invoke(g_readBytes, reader, args, &ok);
    if (!ok || !result) return;
    auto* bytes = reinterpret_cast<Il2CppArray*>(result);
    if (bytes->length != static_cast<uintptr_t>(remaining)) return;
    const auto* memory = reinterpret_cast<const uint8_t*>(arrayData(bytes));
    if (!validWorldNetworkPacket(memory, remaining)) return;
    bl::csharp::onNativeWorldNetReceived(memory, remaining, 256);
}

void hkWorldNetSendData(int message, int remote, int ignore, Il2CppObject* text,
                        int number, float number2, float number3, float number4,
                        int number5, int number6, int number7, const MethodInfo* method) {
    if (!g_originalSendData) return;
    g_originalSendData(message, remote, ignore, text, number, number2, number3,
                       number4, number5, number6, number7, method);
    if (message == 7 && netMode() == 2 &&
        bl::csharp::nativeWorldNetActive())
        bl::csharp::onNativeWorldSyncSent(remote, ignore);
}

bool resolve() {
    auto& api = il2cpp::api();
    Il2CppClass* managerClass = il2cpp::findClass({"Terraria.Net", "NetManager", {}});
    Il2CppClass* packetClass = il2cpp::findClass({"Terraria.Net", "NetPacket", {}});
    Il2CppClass* netMessage = il2cpp::findClass({"Terraria", "NetMessage", {}});
    Il2CppClass* binaryReader = il2cpp::findClass({"System.IO", "BinaryReader", {}});
    Il2CppClass* binaryWriter = il2cpp::findClass({"System.IO", "BinaryWriter", {}});
    Il2CppClass* stream = il2cpp::findClass({"System.IO", "Stream", {}});
    Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
    Il2CppClass* byte = il2cpp::findClass({"System", "Byte", {}});
    if (!managerClass || !packetClass || !netMessage || !binaryReader ||
        !binaryWriter || !stream || !main || !byte) return false;

    auto method = [&](Il2CppClass* type, const char* signature) {
        return il2cpp::findMethodBySignature(type, il2cpp::parseSignature(signature));
    };
    const MethodInfo* read = method(managerClass,
        "void Read(BinaryReader reader, int userId, int readLength)");
    const MethodInfo* send = method(netMessage,
        "void SendData(int msgType, int remoteClient, int ignoreClient, NetworkText text, int number, float number2, float number3, float number4, int number5, int number6, int number7)");
    g_packetCtor = method(packetClass, "void .ctor(ushort id, int size)");
    g_getWriter = api.class_get_method_from_name(packetClass, "get_Writer", 0);
    if (!g_getWriter) g_writerOffset = il2cpp::fieldOffset(packetClass, "Writer");
    g_writeBytes = method(binaryWriter, "void Write(byte[] buffer)");
    g_shrink = api.class_get_method_from_name(packetClass, "ShrinkToFit", 0);
    g_readUInt16 = api.class_get_method_from_name(binaryReader, "ReadUInt16", 0);
    g_readBytes = method(binaryReader, "byte[] ReadBytes(int count)");
    g_getBaseStream = api.class_get_method_from_name(binaryReader, "get_BaseStream", 0);
    g_getPosition = api.class_get_method_from_name(stream, "get_Position", 0);
    g_setPosition = api.class_get_method_from_name(stream, "set_Position", 1);
    g_sendClient = method(managerClass,
        "void SendToClient(NetPacket packet, int clientId)");
    g_broadcast = method(managerClass,
        "void Broadcast(NetPacket packet, int ignoreClient)");
    g_getInstance = api.class_get_method_from_name(managerClass, "get_Instance", 0);
    g_instanceField = il2cpp::findField(managerClass, "Instance");
    g_modeField = il2cpp::findField(main, "netMode");
    g_packetClass = packetClass;
    g_byteClass = byte;
    if (!read || !send || !g_packetCtor ||
        (!g_getWriter && g_writerOffset < 0) || !g_writeBytes ||
        !g_shrink || !g_readUInt16 || !g_readBytes || !g_getBaseStream ||
        !g_getPosition || !g_setPosition || !g_sendClient || !g_broadcast ||
        (!g_getInstance && !g_instanceField) || !g_modeField ||
        !api.method_is_instance || !api.runtime_invoke ||
        !api.object_new || !api.array_new || !api.field_static_get_value ||
        !api.method_is_instance(read) || api.method_is_instance(send) ||
        !api.method_is_instance(g_packetCtor) ||
        !api.method_is_instance(g_readUInt16) ||
        !api.method_is_instance(g_readBytes) ||
        !api.method_is_instance(g_getBaseStream) ||
        !api.method_is_instance(g_getPosition) ||
        !api.method_is_instance(g_setPosition) ||
        !api.method_is_instance(g_writeBytes) ||
        !api.method_is_instance(g_shrink) ||
        !api.method_is_instance(g_sendClient) ||
        !api.method_is_instance(g_broadcast) ||
        (g_getWriter && !api.method_is_instance(g_getWriter)) ||
        (g_getInstance && api.method_is_instance(g_getInstance))) return false;
    if (!bl::hook::install(read, hkWorldNetRead, &g_originalRead)) return false;
    if (!bl::hook::install(send, hkWorldNetSendData, &g_originalSendData)) return false;
    return true;
}
}

bool installNativeWorldNetHooks() {
    static bool attempted = false;
    static bool installed = false;
    if (attempted) return installed;
    attempted = true;
    installed = resolve();
    if (!installed)
        BL_WARN("CSharp WorldNet disabled: Terraria.Net ABI incomplete");
    return installed;
}

bool sendNativeWorldNetPacket(const std::vector<uint8_t>& data,
                              int remoteClient, int ignoreClient,
                              std::string* error) {
    if (!error) return false;
    error->clear();
    if (netMode() != 2 || !validWorldNetworkPacket(data.data(), data.size()) ||
        remoteClient < -1 || remoteClient >= 256 ||
        ignoreClient < -1 || ignoreClient >= 256) {
        *error = "BWN1 server-authority or packet validation rejected";
        return false;
    }
    auto& api = il2cpp::api();
    Il2CppObject* managerObject = manager();
    Il2CppObject* packet = api.object_new(g_packetClass);
    Il2CppArray* bytes = api.array_new(g_byteClass, data.size());
    if (!managerObject || !packet || !bytes) {
        *error = "Terraria network objects unavailable";
        return false;
    }
    std::memcpy(arrayData(bytes), data.data(), data.size());
    uint16_t moduleId = kWorldNetworkModuleId;
    int capacity = static_cast<int>(data.size()) + 6;
    void* ctorArgs[] = {&moduleId, &capacity};
    bool ok = false;
    invoke(g_packetCtor, packet, ctorArgs, &ok);
    if (!ok) { *error = "NetPacket constructor rejected"; return false; }
    Il2CppObject* writer = nullptr;
    if (g_getWriter) writer = invoke(g_getWriter, packet, nullptr, &ok);
    else if (g_writerOffset >= 0) {
        writer = *reinterpret_cast<Il2CppObject**>(
            reinterpret_cast<char*>(packet) + g_writerOffset);
        ok = writer != nullptr;
    }
    if (!ok || !writer) { *error = "NetPacket writer unavailable"; return false; }
    void* writeArgs[] = {bytes};
    invoke(g_writeBytes, writer, writeArgs, &ok);
    if (!ok) { *error = "NetPacket write rejected"; return false; }
    invoke(g_shrink, packet, nullptr, &ok);
    if (!ok) { *error = "NetPacket finalization rejected"; return false; }
    void* sendArgs[] = {packet, &remoteClient};
    if (remoteClient >= 0)
        invoke(g_sendClient, managerObject, sendArgs, &ok);
    else {
        sendArgs[1] = &ignoreClient;
        invoke(g_broadcast, managerObject, sendArgs, &ok);
    }
    if (!ok) { *error = "NetPacket dispatch rejected"; return false; }
    return true;
}
}
