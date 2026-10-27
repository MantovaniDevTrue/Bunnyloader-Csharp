#pragma once

#include "il2cpp/Types.h"

#include <cstdint>

namespace bl::csharp::damage_combat {

using WeaponKnockbackFn = float (*)(Il2CppObject*, Il2CppObject*, float, const MethodInfo*);

// Instala o rastreio por jogador (ResetEffects e fontes de bonus) e a
// velocidade de ataque (ApplyItemAnimation/ApplyItemTime). Thread do jogo.
// Falha de contrato desliga o recurso e mantem o vanilla.
bool installNativeDamageClassTracking();

// Para itens com DamageType customizado, calcula o valor da classe a partir do
// resultado vanilla (item sem classe). false = manter `original`.
bool customWeaponDamage(Il2CppObject* player, Il2CppObject* item, int32_t original, int32_t* out);
bool customWeaponCrit(Il2CppObject* player, Il2CppObject* item, int32_t original, int32_t* out);
bool customWeaponKnockback(Il2CppObject* player, Il2CppObject* item, float knockback,
                           WeaponKnockbackFn vanilla, const MethodInfo* method, float* out);

}  // namespace bl::csharp::damage_combat
