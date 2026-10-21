#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Il2CppObject;

namespace bl::csharp::touch {
struct NativeLayoutBinding {
    std::string profile;
    std::string slot;
    int index = 0;
    int32_t* action = nullptr;
    std::vector<int32_t*> group;
};

// Retorna referências mutáveis ao layout, válidas apenas durante o hook
// síncrono na thread do jogo (sem retê-las depois do callback).
bool enumerateNativeTouchLayouts(Il2CppObject* styles,
                                 std::vector<NativeLayoutBinding>* output);
bool enumerateNativeTouchShareProfile(Il2CppObject* profile,
                                     std::vector<NativeLayoutBinding>* output);
Il2CppObject* currentNativeTouchStyles();
std::string nativeTouchSaveDirectory();
} // namespace bl::csharp::touch
