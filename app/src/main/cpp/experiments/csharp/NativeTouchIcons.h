#pragma once
#include "il2cpp/Types.h"

namespace bl::csharp::touch {

// Somente na thread de jogo. Retorna Texture2D nativa (não Asset<Texture2D>)
// enraizada no GC; cache limitada e sem leitura de disco no caminho de desenho.
Il2CppObject* nativeTouchIcon(int id, int* width, int* height);
void pumpNativeTouchIconsOnGameThread();

} // namespace bl::csharp::touch
