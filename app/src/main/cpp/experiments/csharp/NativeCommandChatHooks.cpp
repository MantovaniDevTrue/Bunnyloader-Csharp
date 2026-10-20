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
FieldInfo* g_netMode = nullptr;
FieldInfo* g_gameMenu = nullptr;
std::atomic<bool> g_incomingInstalled{false};
std::atomic<bool> g_clientInstalled{false};
std::atomic<bool> g_metadataReady{false};
std::atomic<int> g_installAttempts{0};

bool hasSignature(const MethodInfo* method, bool instance,
                  const char* first, const char* second = nullptr) {
    const auto& api = il2cpp::api();
    if (!method || !api.method_is_instance || !api.method_get_param_count ||
        !api.method_get_param || !api.type_get_name || !api.il2cpp_free ||
        api.method_is_instance(method) != instance) return false;
    const unsigned count = second ? 2u : first ? 1u : 0u;
    if (api.method_get_param_count(method) != count) return false;
    const char* expected[] = {first, second};
    for (unsigned i = 0; i < count; ++i) {
        const auto* parameter = api.method_get_param(method, i);
        if (!parameter) return false;
        char* name = api.type_get_name(parameter);
        if (!name) return false;
        const bool same = std::strcmp(name, expected[i]) == 0;
        api.il2cpp_free(name);
        if (!same) return false;
    }
    return true;
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
    return *localPlayer >= 0 && *localPlayer < 255;
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

void hkIncoming(Il2CppObject* self, Il2CppObject* message,
                int clientId, const MethodInfo* method) {
    int mode = -1, localPlayer = -1;
    if (g_metadataReady.load(std::memory_order_acquire) && message &&
        readContext(&mode, &localPlayer) && mode != 1) {
        // No servidor dedicado e para outros jogadores no host, ainda e
        // necessario o transporte de respostas NetworkText. Fail closed:
        // comandos nativos nao interceptam chat remoto nesta fase.
        if (mode == 0 || (mode == 3 && clientId == localPlayer)) {
            auto result = dispatchLocal(message, mode, localPlayer,
                                        NativeCommandRoute::Chat);
            if (result == NativeCommandSubmit::Vanilla && mode == 3)
                result = dispatchLocal(message, mode, localPlayer,
                                       NativeCommandRoute::Server);
            if (result != NativeCommandSubmit::Vanilla) return;
        }
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
    if (++g_installAttempts > 4) return false;
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
    const auto* netMode = il2cpp::findField(main, "netMode");
    const auto* menu = il2cpp::findField(main, "gameMenu");
    if (!hasSignature(incoming, true, "Terraria.Chat.ChatMessage", "System.Int32") ||
        !hasSignature(client, false, "Terraria.Chat.ChatMessage") ||
        !hasSignature(text, true, nullptr) ||
        !hasSignature(consumeMethod, true, nullptr) ||
        !hasSignature(newText, false, "System.String") ||
        !hasSignature(myPlayer, false, nullptr) ||
        !netMode || !menu)
        return false;

    g_textGetter = text;
    g_consume = consumeMethod;
    g_newText = newText;
    g_myPlayer = myPlayer;
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
    for (const auto& message : nativeCommandMailbox().takeReplies(8))
        writeLocal(message.text, message.red, message.green, message.blue);
}

bool nativeCommandChatHookInstalled() {
    return g_metadataReady.load(std::memory_order_acquire);
}

} // namespace bl::csharp
