#pragma once

// Linhas de status do tooltip para itens de DamageClass customizada no
// Terraria Android, como o tModLoader (ItemLoader/TooltipLine): Damage,
// CritChance, Speed e Knockback, cada uma sujeita a ShowStatTooltipLine.
//
// O MouseText_DrawItemTooltip_GetLinesInfo vanilla do celular monta o dano a
// partir dos campos de classe do jogador (melee/ranged/magic/summon) e, para
// item sem flag de classe, escreve "<Item.damage><sufixo>" sem linha de
// critico. Aqui o bloco vanilla (dano, velocidade, knockback) é localizado e
// trocado pelos valores da classe. Sem as linhas esperadas, nada muda.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bl::csharp::damage_combat {

// Bits de ShowStatTooltipLine exportados no snapshot de classes.
inline constexpr uint32_t kTooltipDamage = 1u;
inline constexpr uint32_t kTooltipCritChance = 2u;
inline constexpr uint32_t kTooltipSpeed = 4u;
inline constexpr uint32_t kTooltipKnockback = 8u;
inline constexpr uint32_t kTooltipAll = 15u;

struct TooltipLine {
    std::u16string text;
    bool prefix = false;
    bool bad = false;
};

struct TooltipInput {
    int32_t itemDamage = 0;       // Item.damage (o que o vanilla escreveu)
    int32_t damage = 0;           // GetWeaponDamage da classe
    std::u16string className;     // DisplayName localizado; vazio = sufixo vanilla
    uint32_t show = kTooltipAll;
    bool hasCrit = false;         // UseStandardCritCalcs e sufixo valido
    int32_t crit = 0;
    std::u16string critSuffix;    // Lang.tip[5]: "% critical strike chance"
    int speedIndex = -1;          // 0..7 em speedTexts; -1 = manter o vanilla
    int knockbackIndex = -1;      // 0..8 em knockbackTexts; -1 = manter o vanilla
    std::array<std::u16string, 8> speedTexts;      // Lang.tip[6..13]
    std::array<std::u16string, 9> knockbackTexts;  // Lang.tip[14..22]
};

inline std::u16string asciiU16(const std::string& value) {
    return std::u16string(value.begin(), value.end());
}

// Lang.tip[6..13] do vanilla pelo useAnimation efetivo.
inline int speedTooltipIndex(int32_t frames) {
    if (frames <= 8) return 0;
    if (frames <= 20) return 1;
    if (frames <= 25) return 2;
    if (frames <= 30) return 3;
    if (frames <= 35) return 4;
    if (frames <= 45) return 5;
    if (frames <= 55) return 6;
    return 7;
}

// Lang.tip[14..22] do vanilla pelo knockback efetivo.
inline int knockbackTooltipIndex(float knockback) {
    if (knockback == 0.f) return 0;
    if (knockback <= 1.5f) return 1;
    if (knockback <= 3.f) return 2;
    if (knockback <= 4.f) return 3;
    if (knockback <= 6.f) return 4;
    if (knockback <= 7.f) return 5;
    if (knockback <= 9.f) return 6;
    if (knockback <= 11.f) return 7;
    return 8;
}

// Linha "<Item.damage><sufixo nao numerico>"; devolve o tamanho do numero.
inline size_t damageLinePrefix(const std::u16string& text, int32_t itemDamage) {
    if (itemDamage < 0) return 0;
    const std::u16string digits = asciiU16(std::to_string(itemDamage));
    if (text.size() <= digits.size() || text.compare(0, digits.size(), digits) != 0) return 0;
    const char16_t next = text[digits.size()];
    return next >= u'0' && next <= u'9' ? 0 : digits.size();
}

template <size_t N>
inline bool matchesAny(const std::u16string& text, const std::array<std::u16string, N>& options) {
    for (const auto& option : options)
        if (!option.empty() && option == text) return true;
    return false;
}

// Reescreve o bloco de status. capacity = tamanho dos arrays do jogo.
// false = linhas vanilla nao reconhecidas (nada alterado).
inline bool rewriteDamageClassTooltip(std::vector<TooltipLine>& lines, size_t capacity,
                                      const TooltipInput& in) {
    size_t damageLine = 0, numberSize = 0;
    for (size_t i = 1; i < lines.size(); ++i) {
        if (lines[i].prefix) continue;
        numberSize = damageLinePrefix(lines[i].text, in.itemDamage);
        if (numberSize) { damageLine = i; break; }
    }
    if (!damageLine) return false;
    size_t end = damageLine + 1;
    bool hadSpeed = false, hadKnockback = false;
    if (end < lines.size() && matchesAny(lines[end].text, in.speedTexts)) { hadSpeed = true; ++end; }
    if (end < lines.size() && matchesAny(lines[end].text, in.knockbackTexts)) { hadKnockback = true; ++end; }

    std::vector<TooltipLine> block;
    if (in.show & kTooltipDamage) {
        std::u16string text = asciiU16(std::to_string(in.damage));
        if (in.className.empty()) text += lines[damageLine].text.substr(numberSize);
        else text += u" " + in.className;
        block.push_back({std::move(text)});
    }
    if ((in.show & kTooltipCritChance) && in.hasCrit && !in.critSuffix.empty())
        block.push_back({asciiU16(std::to_string(in.crit)) + in.critSuffix});
    if (hadSpeed && (in.show & kTooltipSpeed)) {
        const auto& text = in.speedIndex >= 0 && in.speedIndex < 8 && !in.speedTexts[static_cast<size_t>(in.speedIndex)].empty()
            ? in.speedTexts[static_cast<size_t>(in.speedIndex)] : lines[damageLine + 1].text;
        block.push_back({text});
    }
    if (hadKnockback && (in.show & kTooltipKnockback)) {
        const auto& vanilla = lines[end - 1].text;
        const auto& text = in.knockbackIndex >= 0 && in.knockbackIndex < 9 &&
                !in.knockbackTexts[static_cast<size_t>(in.knockbackIndex)].empty()
            ? in.knockbackTexts[static_cast<size_t>(in.knockbackIndex)] : vanilla;
        block.push_back({text});
    }
    const size_t removed = end - damageLine;
    if (lines.size() - removed + block.size() > capacity) return false;
    lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(damageLine),
                lines.begin() + static_cast<std::ptrdiff_t>(end));
    lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(damageLine), block.begin(), block.end());
    return true;
}

}  // namespace bl::csharp::damage_combat
