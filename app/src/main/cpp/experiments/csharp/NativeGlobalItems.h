#pragma once

// BunnyGlobalItem (conteúdo) no Terraria Android.
//
// 1. SourceLoader declara os mods que têm GlobalItem de conteúdo (GLOBAL_ITEM).
// 2. Na thread do jogo, o host tira uma foto de cada item vanilla depois do
//    SetDefaults (sem as políticas de GlobalItem) e grava um arquivo.
// 3. O MonoVM do jogo avalia os GlobalItems sobre a foto (uma vez por mod) e
//    entrega o wire GLOBAL_ITEMS_V1.
// 4. A thread do jogo valida, publica e aplica: SetDefaults (campos do
//    NativeDefaults), GetWeaponDamage/Crit/Knockback, tooltip e bônus de
//    equipamento (canal CLASS_BONUS). Itens já existentes são refeitos.

#include "il2cpp/Types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace bl::csharp::global_items {

void declare(const std::string& mod, const std::string& assemblyPath, std::vector<std::string> names);
void unload(const std::string& mod);
bool declared(const std::string& mod);
// Chamado pelo worker do MonoVM com o wire (ou ERROR\t...).
void deliver(const std::string& mod, std::string wire);
// Thread do jogo, a cada quadro.
void pumpOnGameThread();
// Todos os mods declarados já publicaram ou falharam (barreira antes do mundo).
bool ready();

// Aplicação (thread do jogo).
void applyDefaults(Il2CppObject* item, int type);
bool modifyWeaponDamage(Il2CppObject* player, Il2CppObject* item, int32_t* value);
bool modifyWeaponCrit(Il2CppObject* item, int32_t* value);
bool modifyWeaponKnockback(Il2CppObject* item, float* value);
bool installTooltipHook();

}  // namespace bl::csharp::global_items
