#include "script/api/Input.h"

#include <cctype>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace bl::script {

namespace {

struct InputButton {
    std::string id;
    std::string label;
    bool down = false;
    uint16_t pressSerial = 0;
    uint16_t releaseSerial = 0;
};

std::mutex gInputMutex;
std::vector<std::unique_ptr<InputButton>> gInputButtons;

InputButton* findButton(const std::string& id) {
    for (const auto& button : gInputButtons) {
        if (button->id == id) return button.get();
    }
    return nullptr;
}

bool validId(const std::string& id) {
    if (id.empty() || id.size() > 48) return false;
    for (unsigned char c : id) {
        if (!std::isalnum(c) && c != '_' && c != '-' && c != '.') return false;
    }
    return true;
}

bool registerButton(const std::string& id, const std::string& label) {
    if (!validId(id)) return false;
    std::lock_guard<std::mutex> lock(gInputMutex);
    if (InputButton* button = findButton(id)) {
        if (!label.empty() && button->label != label) button->label = label.substr(0, 24);
        return true;
    }
    auto button = std::make_unique<InputButton>();
    button->id = id;
    button->label = label.empty() ? id : label.substr(0, 24);
    gInputButtons.push_back(std::move(button));
    return true;
}

uint32_t packedState(const std::string& id) {
    std::lock_guard<std::mutex> lock(gInputMutex);
    InputButton* button = findButton(id);
    if (!button) return 0;
    return (button->down ? 1u : 0u)
        | ((static_cast<uint32_t>(button->pressSerial) & 0x7FFFu) << 1)
        | ((static_cast<uint32_t>(button->releaseSerial) & 0x7FFFu) << 16);
}

}

std::vector<std::string> modInputButtons() {
    std::lock_guard<std::mutex> lock(gInputMutex);
    std::vector<std::string> out;
    out.reserve(gInputButtons.size() * 2);
    for (const auto& button : gInputButtons) {
        out.push_back(button->id);
        out.push_back(button->label);
    }
    return out;
}

void setModInputState(const std::string& id, bool down) {
    std::lock_guard<std::mutex> lock(gInputMutex);
    InputButton* button = findButton(id);
    if (!button || button->down == down) return;
    button->down = down;
    if (down) button->pressSerial = static_cast<uint16_t>((button->pressSerial + 1) & 0x7FFFu);
    else button->releaseSerial = static_cast<uint16_t>((button->releaseSerial + 1) & 0x7FFFu);
}

#if BL_HAVE_QUICKJS

namespace {

JSValue jsRegisterButton(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "bl.input.registerButton(id, label)");
    }
    const char* rawId = JS_ToCString(ctx, argv[0]);
    if (!rawId) return JS_EXCEPTION;
    std::string id(rawId);
    JS_FreeCString(ctx, rawId);

    std::string label = id;
    if (argc > 1 && JS_IsString(argv[1])) {
        const char* rawLabel = JS_ToCString(ctx, argv[1]);
        if (!rawLabel) return JS_EXCEPTION;
        label = rawLabel;
        JS_FreeCString(ctx, rawLabel);
    }

    if (!registerButton(id, label)) {
        return JS_ThrowRangeError(ctx, "bl.input.registerButton: id invalido");
    }
    return JS_TRUE;
}

JSValue jsState(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsString(argv[0])) {
        return JS_ThrowTypeError(ctx, "bl.input.state(id)");
    }
    const char* rawId = JS_ToCString(ctx, argv[0]);
    if (!rawId) return JS_EXCEPTION;
    std::string id(rawId);
    JS_FreeCString(ctx, rawId);
    return JS_NewUint32(ctx, packedState(id));
}

}

void installInputApi(JSContext* ctx, JSValue bl) {
    static const JSCFunctionListEntry inputFns[] = {
        JS_CFUNC_DEF("registerButton", 2, jsRegisterButton),
        JS_CFUNC_DEF("state", 1, jsState),
    };
    JSValue input = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, input, inputFns, sizeof inputFns / sizeof inputFns[0]);
    JS_SetPropertyStr(ctx, bl, "input", input);
}

#endif

}
