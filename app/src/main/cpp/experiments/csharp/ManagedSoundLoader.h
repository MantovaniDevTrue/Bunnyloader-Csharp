#pragma once

namespace bl::csharp::managedSound {

// Ambos chamados exclusivamente na thread de atualização do Terraria.
// O MonoVM usa somente as funções C ABI definidas em ManagedSoundLoader.cpp.
void pump();
void worldExit();

}
