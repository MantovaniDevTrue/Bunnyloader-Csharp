#pragma once

#include <cstdint>
#include <string>

namespace bl::csharp {

enum NativeContentKind : int32_t {
    NativeProjectile = 1,
    NativeWeapon = 2
};

struct NativeContentSpec {
    int32_t kind;
    const char* name;
    const char* texture;
    const char* displayName;
    int32_t width;
    int32_t height;
    int32_t damage;
    int32_t useTime;
    float shootSpeed;
    int32_t shootType;
    int32_t aiStyle;
    int32_t penetrate;
    int32_t timeLeft;
    int32_t flags;
};

struct NativeRegisterContext {
    std::string modId;
    std::string modDir;
    int items;
    int projectiles;
};

using NativeRegisterCallback = int (*)(void*, const NativeContentSpec*);

bool prepareNativeContent();
int registerNativeContent(void* context, const NativeContentSpec* spec);

}
