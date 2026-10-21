#pragma once
#include <string>

namespace bl::csharp {

// Chamado após BunnyAchievementRuntime.Bind confirmar os metadados.
// Não registra tipos de item e não instala hooks no worker de compilação.
bool registerNativeAchievementMod(const std::string& modId,
                                  const std::string& assemblyPath);

bool nativeAchievementBindingsReady();
bool nativeAchievementHooksReady();

// Deve ser chamado apenas na thread do Terraria, depois de resolver GameRefs.
// Falha fechada se qualquer uma das quatro assinaturas IL2CPP divergir.
bool installNativeAchievementHooks(std::string* error);

// Os ganchos permanecem instalados, mas entregam eventos somente dentro do
// mundo do jogador local. A transição entre mundos altera apenas essa flag.
void setNativeAchievementWorldActive(bool active);

}
