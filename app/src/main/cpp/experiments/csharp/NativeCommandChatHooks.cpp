#include "experiments/csharp/NativeCommandChatHooks.h"

#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/NativeCommandMailbox.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "core/Log.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <limits>
#include <initializer_list>
#include <string>

namespace bl::csharp {
namespace {

using IncomingFn = void (*)(Il2CppObject*, Il2CppObject*, int, const MethodInfo*);
using ClientFn = void (*)(Il2CppObject*, const MethodInfo*);
IncomingFn g_incomingOriginal = nullptr;
ClientFn g_clientOriginal = nullptr;
const MethodInfo* g_textGetter = nullptr;
const MethodInfo* g_consume = nullptr;
const MethodInfo* g_newText = nullptr;
const MethodInfo* g_myPlayer = nullptr;
const MethodInfo* g_networkLiteral = nullptr;
const MethodInfo* g_sendClient = nullptr;
const MethodInfo* g_colorConstructor = nullptr;
Il2CppClass* g_colorClass = nullptr;
FieldInfo* g_netMode = nullptr;
FieldInfo* g_gameMenu = nullptr;
std::atomic<bool> g_incomingInstalled{false};
std::atomic<bool> g_clientInstalled{false};
std::atomic<bool> g_metadataReady{false};
std::atomic<int> g_installAttempts{0};

bool hasSignature(const MethodInfo* method, bool instance,
                  std::initializer_list<const char*> expected) {
    const auto& api = il2cpp::api();
    if (!method || !api.method_is_instance || !api.method_get_param_count ||
        !api.method_get_param || !api.type_get_name || !api.il2cpp_free ||
        api.method_is_instance(method) != instance ||
        api.method_get_param_count(method) != expected.size()) return false;
    unsigned index = 0;
    for (const char* type : expected) {
        const auto* parameter = api.method_get_param(method, index++);
        if (!parameter) return false;
        char* name = api.type_get_name(parameter);
        if (!name) return false;
        const bool same = std::strcmp(name, type) == 0;
        api.il2cpp_free(name);
        if (!same) return false;
    }
    return true;
}

bool hasReturnType(const MethodInfo* method, const char* expected) {
    auto& api = il2cpp::api();
    if (!method || !api.method_get_return_type || !api.type_get_name ||
        !api.il2cpp_free) return false;
    const auto* type = api.method_get_return_type(method);
    if (!type) return false;
    char* name = api.type_get_name(type);
    if (!name) return false;
    const bool same = std::strcmp(name, expected) == 0;
    api.il2cpp_free(name);
    return same;
}

bool utf16ToUtf8(const Il2CppString* raw, std::string* result) {
    if (!raw || !result || raw->length < 0 || raw->length > 512) return false;
    std::string out;
    out.reserve(static_cast<size_t>(raw->length) * 2);
    for (int i = 0; i < raw->length; ++i) {
        uint32_t point = raw->chars[i];
        if (point >= 0xD800 && point <= 0xDBFF) {
            if (++i >= raw->length) return false;
            const uint32_t second = raw->chars[i];
            if (second < 0xDC00 || second > 0xDFFF) return false;
            point = 0x10000 + ((point - 0xD800) << 10) + (second - 0xDC00);
        } else if (point >= 0xDC00 && point <= 0xDFFF) return false;
        if (point <= 0x7F) out.push_back(static_cast<char>(point));
        else if (point <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (point >> 6)));
            out.push_back(static_cast<char>(0x80 | (point & 63)));
        } else if (point <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (point >> 12)));
            out.push_back(static_cast<char>(0x80 | ((point >> 6) & 63)));
            out.push_back(static_cast<char>(0x80 | (point & 63)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (point >> 18)));
            out.push_back(static_cast<char>(0x80 | ((point >> 12) & 63)));
            out.push_back(static_cast<char>(0x80 | ((point >> 6) & 63)));
            out.push_back(static_cast<char>(0x80 | (point & 63)));
        }
        if (out.size() > NativeCommandMailbox::MaxInputBytes) return false;
    }
    *result = std::move(out);
    return true;
}

bool readText(Il2CppObject* message, std::string* output) {
    if (!message || !g_textGetter) return false;
    auto& api = il2cpp::api();
    Il2CppObject* exception = nullptr;
    Il2CppObject* value = api.runtime_invoke(g_textGetter, message, nullptr, &exception);
    return !exception && utf16ToUtf8(reinterpret_cast<Il2CppString*>(value), output);
}

bool readContext(int* mode, int* localPlayer) {
    if (!mode || !localPlayer || !g_netMode || !g_gameMenu || !g_myPlayer)
        return false;
    auto& api = il2cpp::api();
    bool inMenu = true;
    api.field_static_get_value(g_gameMenu, &inMenu);
    if (inMenu) return false;
    api.field_static_get_value(g_netMode, mode);
    if (*mode < 0 || *mode > 3) return false;
    Il2CppObject* exception = nullptr;
    Il2CppObject* boxed = api.runtime_invoke(g_myPlayer, nullptr, nullptr, &exception);
    if (exception || !boxed) return false;
    std::memcpy(localPlayer, reinterpret_cast<const char*>(boxed) + sizeof(Il2CppObject),
                sizeof(*localPlayer));
    return *localPlayer >= 0 && *localPlayer <= 255;
}

bool consume(Il2CppObject* message) {
    auto& api = il2cpp::api();
    Il2CppObject* exception = nullptr;
    api.runtime_invoke(g_consume, message, nullptr, &exception);
    return !exception;
}

void writeLocal(const std::string& message, uint8_t red, uint8_t green, uint8_t blue) {
    if (!g_newText) return;
    auto& api = il2cpp::api();
    Il2CppString* value = api.string_new(message.c_str());
    if (!value) return;
    bool onlyCurrent = false;
    void* args[] = {value, &red, &green, &blue, &onlyCurrent};
    Il2CppObject* exception = nullptr;
    api.runtime_invoke(g_newText, nullptr, args, &exception);
}

bool writeNetwork(const std::string& message, int playerIndex,
                  uint8_t red, uint8_t green, uint8_t blue) {
    if (!g_networkLiteral || !g_sendClient || !g_colorConstructor ||
        !g_colorClass || playerIndex < 0 || playerIndex >= 255) return false;
    auto& api = il2cpp::api();
    Il2CppString* text = api.string_new(message.c_str());
    if (!text) return false;
    void* makeTextArgs[] = {text};
    Il2CppObject* exception = nullptr;
    Il2CppObject* network = api.runtime_invoke(
        g_networkLiteral, nullptr, makeTextArgs, &exception);
    if (exception || !network) return false;
    Il2CppObject* color = api.object_new(g_colorClass);
    if (!color) return false;
    uint8_t alpha = 255;
    void* colorArgs[] = {&red, &green, &blue, &alpha};
    exception = nullptr;
    api.runtime_invoke(g_colorConstructor, color, colorArgs, &exception);
    if (exception) return false;
    uint32_t rgba = 0;
    std::memcpy(&rgba, reinterpret_cast<const char*>(color) + sizeof(Il2CppObject),
                sizeof(rgba));
    void* sendArgs[] = {network, &rgba, &playerIndex};
    exception = nullptr;
    api.runtime_invoke(g_sendClient, nullptr, sendArgs, &exception);
    return !exception;
}

// Estes efeitos so sao chamados pelo pump da thread do Terraria. Todos
// os campos e metodos sao resolvidos a partir do jogo; Mono nunca ve Player*.
bool applyNativeHeal(const NativeCommandAction& action, std::string* confirmation) {
    auto& api = il2cpp::api();
    if (!api.runtime_invoke || !api.field_static_get_value || !api.field_get_offset)
        return false;
    auto* main = il2cpp::findClassQuiet("Terraria", "Main");
    auto* playerClass = il2cpp::findClassQuiet("Terraria", "Player");
    if (!main || !playerClass) return false;
    FieldInfo* playersField = il2cpp::findField(main, "player");
    FieldInfo* lifeField = il2cpp::findField(playerClass, "statLife");
    FieldInfo* maxField = il2cpp::findField(playerClass, "statLifeMax2");
    FieldInfo* activeField = il2cpp::findField(playerClass, "active");
    const auto* healEffect = api.class_get_method_from_name(playerClass, "HealEffect", 2);
    if (!maxField) maxField = il2cpp::findField(playerClass, "statLifeMax");
    if (!playersField || !lifeField || !maxField || !activeField ||
        !hasSignature(healEffect, true, {"System.Int32", "System.Boolean"}) ||
        !hasReturnType(healEffect, "System.Void")) return false;
    Il2CppArray* players = nullptr;
    api.field_static_get_value(playersField, &players);
    if (!players || action.playerIndex < 0 ||
        static_cast<uintptr_t>(action.playerIndex) >= players->length ||
        players->length > 256) return false;
    Il2CppObject* player = static_cast<Il2CppObject**>(arrayData(players))[action.playerIndex];
    if (!player) return false;
    const size_t activeOffset = api.field_get_offset(activeField);
    const size_t lifeOffset = api.field_get_offset(lifeField);
    const size_t maxOffset = api.field_get_offset(maxField);
    if (activeOffset < sizeof(Il2CppObject) || activeOffset > 16384 ||
        lifeOffset < sizeof(Il2CppObject) || lifeOffset > 16384 ||
        maxOffset < sizeof(Il2CppObject) || maxOffset > 16384) return false;
    uint8_t active = 0;
    int life = 0, maximum = 0;
    std::memcpy(&active, reinterpret_cast<const char*>(player) + activeOffset, 1);
    std::memcpy(&life, reinterpret_cast<const char*>(player) + lifeOffset, sizeof(int));
    std::memcpy(&maximum, reinterpret_cast<const char*>(player) + maxOffset, sizeof(int));
    if (!active || life < 0 || maximum <= 0 || maximum > 100000 ||
        life > maximum || action.amount <= 0 || action.amount > 1000000)
        return false;
    const int healed = std::min(action.amount, maximum - life);
    if (healed <= 0) {
        *confirmation = "Vida cheia";
        return true;
    }
    const int updated = life + healed;
    std::memcpy(reinterpret_cast<char*>(player) + lifeOffset, &updated, sizeof(updated));
    int quantity = healed;
    bool broadcast = true;
    void* args[] = {&quantity, &broadcast};
    Il2CppObject* exception = nullptr;
    api.runtime_invoke(healEffect, player, args, &exception);
    if (exception) {
        std::memcpy(reinterpret_cast<char*>(player) + lifeOffset, &life, sizeof(life));
        return false;
    }
    *confirmation = "Curado: " + std::to_string(healed);
    return true;
}

bool applyNativeWorldTime(const NativeCommandAction& action) {
    auto& api = il2cpp::api();
    if (!api.field_static_get_value || !api.field_static_set_value ||
        action.amount < -86400000 || action.amount > 86400000)
        return false;
    auto* main = il2cpp::findClassQuiet("Terraria", "Main");
    if (!main) return false;
    FieldInfo* timeField = il2cpp::findField(main, "time");
    FieldInfo* dayField = il2cpp::findField(main, "dayTime");
    if (!timeField || !dayField || !api.field_get_type ||
        !api.type_get_name || !api.il2cpp_free) return false;
    auto typeIs = [&](FieldInfo* field, const char* expected) {
        const auto* fieldType = api.field_get_type(field);
        char* text = fieldType ? api.type_get_name(fieldType) : nullptr;
        if (!text) return false;
        const bool valid = std::strcmp(text, expected) == 0;
        api.il2cpp_free(text);
        return valid;
    };
    if (!typeIs(timeField, "System.Double") || !typeIs(dayField, "System.Boolean"))
        return false;
    // Comprimentos do ciclo sao os usados pelo ExampleTimeCommand Bunny.
    constexpr double dayLength = 54000.0;
    constexpr double nightLength = 32400.0;
    constexpr double cycle = dayLength + nightLength;
    double oldTime = 0;
    bool oldDay = false;
    api.field_static_get_value(timeField, &oldTime);
    api.field_static_get_value(dayField, &oldDay);
    if (!std::isfinite(oldTime) || oldTime < 0 || oldTime > cycle)
        return false;

    const MethodInfo* sendData = nullptr;
    if ((action.networkMode & 2) != 0) {
        auto* netMessage = il2cpp::findClassQuiet("Terraria", "NetMessage");
        sendData = netMessage
            ? api.class_get_method_from_name(netMessage, "SendData", 11) : nullptr;
        if (!hasSignature(sendData, false, {"System.Int32", "System.Int32",
                                           "System.Int32", "Terraria.Localization.NetworkText",
                                           "System.Int32", "System.Single", "System.Single",
                                           "System.Single", "System.Int32", "System.Int32",
                                           "System.Int32"}) ||
            !hasReturnType(sendData, "System.Void")) return false;
    }
    double absolute = std::fmod(oldTime + (oldDay ? 0 : dayLength) +
                                static_cast<double>(action.amount), cycle);
    if (absolute < 0) absolute += cycle;
    const bool nextDay = absolute < dayLength;
    const double nextTime = nextDay ? absolute : absolute - dayLength;
    api.field_static_set_value(dayField, const_cast<bool*>(&nextDay));
    api.field_static_set_value(timeField, const_cast<double*>(&nextTime));
    if (sendData) {
        // MessageID.WorldData do Terraria vanilla, como no ExampleTimeCommand.
        int worldData = 7, remote = -1, ignore = -1, zero = 0;
        float zeroFloat = 0.f;
        void* args[] = {&worldData, &remote, &ignore, nullptr, &zero,
                        &zeroFloat, &zeroFloat, &zeroFloat,
                        &zero, &zero, &zero};
        Il2CppObject* exception = nullptr;
        api.runtime_invoke(sendData, nullptr, args, &exception);
        if (exception) {
            api.field_static_set_value(dayField, &oldDay);
            api.field_static_set_value(timeField, &oldTime);
            return false;
        }
    }
    return true;
}

void applyNativeCommandActionsOnGameThread() {
    int mode = -1, localPlayer = -1;
    if (!readContext(&mode, &localPlayer)) return;
    for (const auto& action : nativeCommandMailbox().takeActions(8)) {
        if (action.epoch != nativeCommandMailbox().epoch() ||
            action.networkMode != mode) continue;
        bool accepted = false;
        std::string reply;
        if (action.kind == NativeCommandActionKind::Heal &&
            action.caller == NativeCommandRoute::Chat &&
            (mode == 0 || mode == 1 || (mode == 3 && action.playerIndex == localPlayer)))
            accepted = applyNativeHeal(action, &reply);
        else if (action.kind == NativeCommandActionKind::AdvanceWorldTime &&
                 (mode == 0 && action.caller == NativeCommandRoute::Chat ||
                  (mode & 2) && action.caller == NativeCommandRoute::Server))
            accepted = applyNativeWorldTime(action);
        if (!accepted) reply = "Comando C#: falha ao aplicar efeito";
        if (!reply.empty()) {
            if ((mode & 2) && (mode != 3 || action.playerIndex != localPlayer))
                writeNetwork(reply, action.playerIndex,
                             accepted ? 124 : 255, accepted ? 252 : 25,
                             accepted ? 0 : 25);
            else
                writeLocal(reply, accepted ? 124 : 255,
                           accepted ? 252 : 25, accepted ? 0 : 25);
        }
    }
}

NativeCommandSubmit dispatchLocal(Il2CppObject* message, int netMode,
                                   int localPlayer, NativeCommandRoute route) {
    std::string input;
    if (!readText(message, &input)) return NativeCommandSubmit::Vanilla;
    if (input.find_first_not_of(" \t\r\n") == std::string::npos)
        return NativeCommandSubmit::Vanilla;
    const auto status = nativeCommandMailbox().submit(
        input, localPlayer, netMode, route);
    if (status != NativeCommandSubmit::Vanilla) {
        consume(message);
        if (status == NativeCommandSubmit::Busy)
            writeLocal("Fila de comandos C# ocupada", 255, 25, 25);
        else
            notifyManagedCommandWorker();
    }
    return status;
}

NativeCommandSubmit dispatchRemote(Il2CppObject* message, int netMode, int playerIndex) {
    if (playerIndex < 0 || playerIndex >= 255) return NativeCommandSubmit::Vanilla;
    std::string input;
    if (!readText(message, &input)) return NativeCommandSubmit::Vanilla;
    const auto result = nativeCommandMailbox().submit(
        input, playerIndex, netMode, NativeCommandRoute::Server);
    if (result != NativeCommandSubmit::Vanilla) {
        consume(message);
        if (result == NativeCommandSubmit::Busy)
            writeNetwork("Fila de comandos C# ocupada", playerIndex, 255, 25, 25);
        else
            notifyManagedCommandWorker();
    }
    return result;
}

void hkIncoming(Il2CppObject* self, Il2CppObject* message,
                int clientId, const MethodInfo* method) {
    int mode = -1, localPlayer = -1;
    if (g_metadataReady.load(std::memory_order_acquire) && message &&
        readContext(&mode, &localPlayer) && mode != 1) {
        if (mode == 0 || (mode == 3 && clientId == localPlayer)) {
            auto result = dispatchLocal(message, mode, localPlayer,
                                        NativeCommandRoute::Chat);
            if (result == NativeCommandSubmit::Vanilla && mode == 3)
                result = dispatchLocal(message, mode, localPlayer,
                                       NativeCommandRoute::Server);
            if (result != NativeCommandSubmit::Vanilla) return;
        } else if ((mode & 2) != 0 &&
                   dispatchRemote(message, mode, clientId) !=
                       NativeCommandSubmit::Vanilla) return;
    }
    if (g_incomingOriginal) g_incomingOriginal(self, message, clientId, method);
}

void hkClient(Il2CppObject* message, const MethodInfo* method) {
    int mode = -1, localPlayer = -1;
    if (g_metadataReady.load(std::memory_order_acquire) && message &&
        readContext(&mode, &localPlayer) && mode == 1 &&
        dispatchLocal(message, mode, localPlayer, NativeCommandRoute::Chat) !=
            NativeCommandSubmit::Vanilla)
        return;
    if (g_clientOriginal) g_clientOriginal(message, method);
}

} // namespace

bool installNativeCommandChatHooks() {
    if (g_metadataReady.load(std::memory_order_acquire)) return true;
    if (++g_installAttempts > 16) return false;
    auto& api = il2cpp::api();
    if (!api.runtime_invoke || !api.string_new || !api.field_static_get_value ||
        !api.class_get_method_from_name) return false;

    auto* processor = il2cpp::findClassQuiet("Terraria.Chat", "ChatCommandProcessor");
    auto* chatHelper = il2cpp::findClassQuiet("Terraria.Chat", "ChatHelper");
    auto* message = il2cpp::findClassQuiet("Terraria.Chat", "ChatMessage");
    auto* main = il2cpp::findClassQuiet("Terraria", "Main");
    if (!processor || !chatHelper || !message || !main) return false;
    const auto* incoming = api.class_get_method_from_name(processor,
        "ProcessIncomingMessage", 2);
    const auto* client = api.class_get_method_from_name(chatHelper,
        "SendChatMessageFromClient", 1);
    const auto* text = api.class_get_method_from_name(message, "get_Text", 0);
    const auto* consumeMethod = api.class_get_method_from_name(message, "Consume", 0);
    const auto* newText = api.class_get_method_from_name(main, "NewText", 5);
    const auto* myPlayer = api.class_get_method_from_name(main, "get_myPlayer", 0);
    auto* networkText = il2cpp::findClassQuiet("Terraria.Localization", "NetworkText");
    auto* color = il2cpp::findClassQuiet("Microsoft.Xna.Framework", "Color");
    const auto* literal = networkText ? api.class_get_method_from_name(
        networkText, "FromLiteral", 1) : nullptr;
    const auto* send = api.class_get_method_from_name(
        chatHelper, "SendChatMessageToClient", 3);
    const auto* colorConstructor = color ? api.class_get_method_from_name(
        color, ".ctor", 4) : nullptr;
    uint32_t alignment = 0;
    const bool validColor = color && api.class_value_size && api.object_new &&
        api.class_value_size(color, &alignment) == 4;
    const auto* netMode = il2cpp::findField(main, "netMode");
    const auto* menu = il2cpp::findField(main, "gameMenu");
    if (!hasSignature(incoming, true, {"Terraria.Chat.ChatMessage", "System.Int32"}) ||
        !hasSignature(client, false, {"Terraria.Chat.ChatMessage"}) ||
        !hasSignature(text, true, {}) ||
        !hasSignature(consumeMethod, true, {}) ||
        !hasSignature(newText, false, {"System.String", "System.Byte", "System.Byte", "System.Byte", "System.Boolean"}) ||
        !hasSignature(myPlayer, false, {}) ||
        !hasSignature(literal, false, {"System.String"}) ||
        !hasSignature(send, false, {"Terraria.Localization.NetworkText",
                                    "Microsoft.Xna.Framework.Color", "System.Int32"}) ||
        !hasSignature(colorConstructor, true,
                      {"System.Byte", "System.Byte", "System.Byte", "System.Byte"}) ||
        !hasReturnType(incoming, "System.Void") ||
        !hasReturnType(client, "System.Void") ||
        !hasReturnType(text, "System.String") ||
        !hasReturnType(consumeMethod, "System.Void") ||
        !hasReturnType(newText, "System.Void") ||
        !hasReturnType(myPlayer, "System.Int32") ||
        !hasReturnType(literal, "Terraria.Localization.NetworkText") ||
        !hasReturnType(send, "System.Void") ||
        !hasReturnType(colorConstructor, "System.Void") ||
        !validColor || !netMode || !menu)
        return false;

    g_textGetter = text;
    g_consume = consumeMethod;
    g_newText = newText;
    g_myPlayer = myPlayer;
    g_networkLiteral = literal;
    g_sendClient = send;
    g_colorConstructor = colorConstructor;
    g_colorClass = color;
    g_netMode = const_cast<FieldInfo*>(netMode);
    g_gameMenu = const_cast<FieldInfo*>(menu);
    if (!g_incomingInstalled.load(std::memory_order_acquire) &&
        hook::install(incoming, hkIncoming, &g_incomingOriginal))
        g_incomingInstalled.store(true, std::memory_order_release);
    if (!g_clientInstalled.load(std::memory_order_acquire) &&
        hook::install(client, hkClient, &g_clientOriginal))
        g_clientInstalled.store(true, std::memory_order_release);
    // Hooks parciais ficam desarmados. Nenhuma mensagem e consumida.
    const bool ready = g_incomingInstalled.load(std::memory_order_acquire) &&
                       g_clientInstalled.load(std::memory_order_acquire);
    g_metadataReady.store(ready, std::memory_order_release);
    return ready;
}

void pumpNativeCommandRepliesOnGameThread() {
    if (!g_metadataReady.load(std::memory_order_acquire)) return;
    int mode = -1, localPlayer = -1;
    if (!readContext(&mode, &localPlayer)) return;
    applyNativeCommandActionsOnGameThread();
    for (const auto& message : nativeCommandMailbox().takeReplies(8)) {
        if (mode & 2) {
            if (mode == 3 && message.playerIndex == localPlayer)
                writeLocal(message.text, message.red, message.green, message.blue);
            else
                writeNetwork(message.text, message.playerIndex,
                             message.red, message.green, message.blue);
        } else
            writeLocal(message.text, message.red, message.green, message.blue);
    }
}

bool nativeCommandChatHookInstalled() {
    return g_metadataReady.load(std::memory_order_acquire);
}

} // namespace bl::csharp
