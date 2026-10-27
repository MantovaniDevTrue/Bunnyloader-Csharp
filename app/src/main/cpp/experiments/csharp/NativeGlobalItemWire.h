#pragma once

// Wire GLOBAL_ITEMS_V1 produzido por BunnyGlobalItemRuntime (MonoVM) e as
// regras puras de aplicação nos GetWeapon*. Sem IL2CPP: testável no host.
//
//   GLOBAL_ITEMS_V1
//   DEFAULTS   <type> <campo:tipo:valor;...>          (formato do NativeDefaults)
//   TOOLTIP    <type> <hex utf8>,<hex utf8>...        (até 8 linhas, 120 bytes)
//   DAMAGE     <type> <base> <add> <mul> <flat>       (StatModifier)
//   CRIT       <type> <valor>
//   KNOCKBACK  <type> <base> <add> <mul> <flat>
//   CLASS_BONUS <type> <classe> <11 números do CLASS_BONUS do BunnyItem>
//   SHOOT      <type> <máscara 1..7> @GlobalItem:<Classe>[,<Classe>...]
//              (CanShoot=1, ModifyShootStats=2, Shoot=4; dono do NativeShooting)

#include <charconv>
#include <cmath>
#include <cstdint>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace bl::csharp::global_items {

inline constexpr int kMaxVanillaType = 16384;

struct Modifier {
    float base = 0.f, additive = 1.f, multiplicative = 1.f, flat = 0.f;
    bool neutral() const { return base == 0.f && additive == 1.f && multiplicative == 1.f && flat == 0.f; }
};

struct ClassBonusWire {
    std::string classKey;
    float values[11]{};  // dmg base/add/mul/flat, kb base/add/mul/flat, crit, attackSpeed, armorPen
};

struct TypeWire {
    std::string defaults;                // descriptor do NativeDefaults
    std::vector<std::string> tooltips;   // UTF-8
    bool hasDamage = false, hasKnockback = false;
    Modifier damage, knockback;
    float crit = 0.f;
    std::vector<ClassBonusWire> classBonuses;
    int shootMask = 0;                   // 0 = sem callbacks de disparo
    std::string shootOwner;              // @GlobalItem:Ns.Classe[,Ns.Outra]
};

inline constexpr std::string_view kGlobalShootOwnerPrefix = "@GlobalItem:";

// Dono enviado ao BunnyShootRuntime: nomes completos de classes C#.
inline bool validShootOwner(std::string_view owner) {
    if (!owner.starts_with(kGlobalShootOwnerPrefix) || owner.size() > 1024 ||
        owner.size() == kGlobalShootOwnerPrefix.size()) return false;
    const auto names = owner.substr(kGlobalShootOwnerPrefix.size());
    size_t start = 0;
    for (size_t i = 0; i <= names.size(); ++i) {
        if (i < names.size() && names[i] != ',') {
            const unsigned char c = static_cast<unsigned char>(names[i]);
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                  c == '_' || c == '.' || c == '+')) return false;
            continue;
        }
        if (i == start) return false;  // nome vazio
        start = i + 1;
    }
    return true;
}

using WireTable = std::map<int, TypeWire>;

inline bool parseNumber(std::string_view text, float* out) {
    if (text.empty() || text.size() > 32 || !out) return false;
    std::istringstream input{std::string(text)};
    input.imbue(std::locale::classic());
    input >> std::noskipws >> *out;
    return !input.fail() && input.eof() && std::isfinite(*out) && std::abs(*out) <= 100000.f;
}

inline bool parseType(std::string_view text, int* out) {
    int value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        value < 1 || value >= kMaxVanillaType) return false;
    *out = value;
    return true;
}

inline bool decodeHex(std::string_view hex, std::string* out) {
    if (hex.empty() || hex.size() % 2 || hex.size() > 240) return false;
    out->clear();
    for (size_t i = 0; i < hex.size(); i += 2) {
        auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        const int hi = nibble(hex[i]), lo = nibble(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        const char byte = static_cast<char>(hi * 16 + lo);
        if (static_cast<unsigned char>(byte) < 32 || byte == 127) return false;
        out->push_back(byte);
    }
    return true;
}

inline std::vector<std::string_view> splitView(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); ++i)
        if (i == text.size() || text[i] == separator) {
            parts.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    return parts;
}

// Decodifica o wire inteiro; qualquer linha inválida rejeita o lote.
inline bool parseWire(std::string_view wire, WireTable* out, std::string* error) {
    auto fail = [&](const std::string& why) { if (error) *error = why; return false; };
    if (!out) return fail("NoOutput");
    if (wire.size() > (1u << 20)) return fail("WireTooLarge");
    if (wire.starts_with("ERROR\t")) {
        auto line = wire.substr(6);
        const size_t end = line.find('\n');
        return fail("Managed:" + std::string(line.substr(0, end == std::string_view::npos ? 240 : std::min<size_t>(end, 240))));
    }
    if (!wire.starts_with("GLOBAL_ITEMS_V1\n")) return fail("MissingHeader");
    wire.remove_prefix(16);
    WireTable table;
    std::set<std::pair<std::string, int>> seen;
    size_t lines = 0;
    while (!wire.empty()) {
        const size_t end = wire.find('\n');
        if (end == std::string_view::npos) return fail("UnterminatedLine");
        const auto line = wire.substr(0, end);
        wire.remove_prefix(end + 1);
        if (line.empty()) continue;
        if (++lines > 65536) return fail("TooManyLines");
        const auto cells = splitView(line, '\t');
        int type = 0;
        if (cells.size() < 3 || !parseType(cells[1], &type)) return fail("InvalidType");
        const std::string kind(cells[0]);
        if (kind != "CLASS_BONUS" && !seen.emplace(kind, type).second) return fail("Duplicate:" + kind);
        auto& entry = table[type];
        if (kind == "DEFAULTS") {
            if (cells.size() != 3 || cells[2].empty() || cells[2].size() > 4096) return fail("InvalidDefaults");
            entry.defaults = std::string(cells[2]);
        } else if (kind == "TOOLTIP") {
            if (cells.size() != 3) return fail("InvalidTooltip");
            const auto parts = splitView(cells[2], ',');
            if (parts.empty() || parts.size() > 8) return fail("TooManyTooltipLines");
            for (const auto part : parts) {
                std::string text;
                if (!decodeHex(part, &text)) return fail("InvalidTooltipText");
                entry.tooltips.push_back(std::move(text));
            }
        } else if (kind == "DAMAGE" || kind == "KNOCKBACK") {
            Modifier m;
            if (cells.size() != 6 || !parseNumber(cells[2], &m.base) || !parseNumber(cells[3], &m.additive) ||
                !parseNumber(cells[4], &m.multiplicative) || !parseNumber(cells[5], &m.flat) ||
                m.multiplicative < 0.f) return fail("InvalidModifier");
            if (kind == "DAMAGE") { entry.damage = m; entry.hasDamage = true; }
            else { entry.knockback = m; entry.hasKnockback = true; }
        } else if (kind == "CRIT") {
            if (cells.size() != 3 || !parseNumber(cells[2], &entry.crit)) return fail("InvalidCrit");
        } else if (kind == "CLASS_BONUS") {
            ClassBonusWire bonus;
            if (cells.size() != 14 || cells[2].empty() || cells[2].size() > 242) return fail("InvalidClassBonus");
            bonus.classKey = std::string(cells[2]);
            for (size_t i = 0; i < 11; ++i)
                if (!parseNumber(cells[3 + i], &bonus.values[i])) return fail("InvalidClassBonusValue");
            if (entry.classBonuses.size() >= 16) return fail("TooManyClassBonuses");
            entry.classBonuses.push_back(std::move(bonus));
        } else if (kind == "SHOOT") {
            int mask = 0;
            const auto result = cells.size() == 4
                ? std::from_chars(cells[2].data(), cells[2].data() + cells[2].size(), mask)
                : std::from_chars_result{nullptr, std::errc::invalid_argument};
            if (result.ec != std::errc{} || result.ptr != cells[2].data() + cells[2].size() ||
                mask < 1 || mask > 7 || !validShootOwner(cells[3])) return fail("InvalidShoot");
            entry.shootMask = mask;
            entry.shootOwner = std::string(cells[3]);
        } else {
            return fail("UnknownKind:" + kind);
        }
    }
    *out = std::move(table);
    return true;
}

// tModLoader: GetWeaponDamage = (int)(StatModifier.ApplyTo(damage) + 5E-06f).
// No Android o campo de classe (meleeDamage etc., 1 sem classe) é o aditivo
// vanilla; o modificador do GlobalItem se soma a ele (CombineWith).
inline int32_t weaponDamage(int32_t itemDamage, float classField, const Modifier& m, int32_t fallback) {
    if (itemDamage < 0 || !std::isfinite(classField) || classField <= 0.f) return fallback;
    const float additive = classField + m.additive - 1.f;
    const float value = (static_cast<float>(itemDamage) + m.base) * additive * m.multiplicative + m.flat + 5e-06f;
    if (!std::isfinite(value) || value < 0.f || value > 2147483000.f) return fallback;
    return static_cast<int32_t>(value);
}

inline float weaponKnockback(float vanilla, const Modifier& m) {
    if (!std::isfinite(vanilla)) return vanilla;
    const float value = (vanilla + m.base) * m.additive * m.multiplicative + m.flat;
    return std::isfinite(value) && value >= 0.f && value <= 100000.f ? value : vanilla;
}

}  // namespace bl::csharp::global_items
