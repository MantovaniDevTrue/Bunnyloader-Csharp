#pragma once

namespace bl::csharp {

// Instala as rotas C# na thread do jogo somente com metadados IL2CPP
// consistentes; em qualquer falha os comandos vanilla permanecem intactos.
bool installNativeCommandChatHooks();
void pumpNativeCommandRepliesOnGameThread();
bool nativeCommandChatHookInstalled();

}
