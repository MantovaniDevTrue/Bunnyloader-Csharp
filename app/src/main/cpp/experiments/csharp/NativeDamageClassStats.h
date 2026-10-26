#pragma once

#include <array>
#include <atomic>
#include <locale>
#include <sstream>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bl::csharp {

struct NativeDamageStatModifier {
    float base = 0.f;
    float additive = 1.f;
    float multiplicative = 1.f;
    float flat = 0.f;
};

struct NativeDamageInheritance {
    float damage = 0.f;
    float crit = 0.f;
    float attackSpeed = 0.f;
    float armorPenetration = 0.f;
    float knockback = 0.f;
};

struct NativeDamageClassStatDefaults {
    NativeDamageStatModifier damage;
    NativeDamageStatModifier knockback;
    float crit = 0.f;
    float attackSpeed = 1.f;
    float armorPenetration = 0.f;
    bool standardCrit = true;
    std::array<NativeDamageInheritance, 5> inheritance{};
};

class NativeDamageClassStatRegistry {
    using Key = std::pair<std::string, std::string>;
    using Stats = std::map<Key, NativeDamageClassStatDefaults>;
    mutable std::mutex mutex_;
    std::shared_ptr<const Stats> active_ = std::make_shared<const Stats>();

    static bool number(std::string_view source, float* value) {
        if (!value || source.empty() || source.size() > 32) return false;
        std::istringstream input{std::string(source)};
        input.imbue(std::locale::classic());
        input >> std::noskipws >> *value;
        return !input.fail() && input.eof() &&
               std::isfinite(*value) && std::abs(*value) <= 100000.f;
    }

public:
    bool publish(const std::string& mod, std::string_view wire) {
        if (mod.empty() || mod.size() > 128 || wire.size() > 32768 ||
            !wire.starts_with("DAMAGE_CLASS_STATS_V2\n")) return false;
        wire.remove_prefix(sizeof("DAMAGE_CLASS_STATS_V2\n") - 1);
        Stats incoming;
        int rows = 0;
        while (!wire.empty()) {
            if (++rows > 118) return false;
            const size_t end = wire.find('\n');
            if (end == std::string_view::npos) return false;
            auto line = wire.substr(0, end);
            wire.remove_prefix(end + 1);
            if (line.empty()) return false;
            std::vector<std::string_view> columns;
            while (true) {
                size_t tab = line.find('\t');
                columns.push_back(line.substr(0, tab));
                if (tab == std::string_view::npos) break;
                line.remove_prefix(tab + 1);
            }
            if (columns.size() != 39 || columns[0] != "CLASS" ||
                columns[1].empty() || columns[1].size() > 240 ||
                (columns[13] != "0" && columns[13] != "1")) return false;
            float parsed[11]{};
            for (size_t index = 0; index < 11; ++index)
                if (!number(columns[index + 2], &parsed[index])) return false;
            if (parsed[9] < 0.f) return false;
            NativeDamageClassStatDefaults stats{
                {parsed[0], parsed[1], parsed[2], parsed[3]},
                {parsed[4], parsed[5], parsed[6], parsed[7]},
                parsed[8], parsed[9], parsed[10], columns[13] == "1"
            };
            for (size_t source = 0; source < 5; ++source) {
                float part[5]{};
                for (size_t attribute = 0; attribute < 5; ++attribute)
                    if (!number(columns[14 + source * 5 + attribute], &part[attribute]))
                        return false;
                stats.inheritance[source] = {
                    part[0], part[1], part[2], part[3], part[4]
                };
            }
            if (!incoming.emplace(Key{mod, "C:" + std::string(columns[1])}, stats).second)
                return false;
        }
        std::lock_guard guard(mutex_);
        auto next = std::make_shared<Stats>(*std::atomic_load_explicit(
            &active_, std::memory_order_acquire));
        for (const auto& [key, stats] : incoming)
            if (!next->emplace(key, stats).second) return false;
        std::atomic_store_explicit(&active_, std::shared_ptr<const Stats>(std::move(next)),
                                   std::memory_order_release);
        return true;
    }

    bool lookup(const std::string& mod, std::string_view cls,
                NativeDamageClassStatDefaults* output) const {
        if (!output || mod.empty() || cls.empty()) return false;
        const auto view = std::atomic_load_explicit(&active_, std::memory_order_acquire);
        const auto found = view->find(Key{mod, std::string(cls)});
        if (found == view->end()) return false;
        *output = found->second;
        return true;
    }

    void unload(const std::string& mod) {
        std::lock_guard guard(mutex_);
        auto next = std::make_shared<Stats>(*std::atomic_load_explicit(
            &active_, std::memory_order_acquire));
        for (auto it = next->begin(); it != next->end();)
            if (it->first.first == mod) it = next->erase(it);
            else ++it;
        std::atomic_store_explicit(&active_, std::shared_ptr<const Stats>(std::move(next)),
                                   std::memory_order_release);
    }

    void reset() {
        std::lock_guard guard(mutex_);
        std::atomic_store_explicit(&active_, std::make_shared<const Stats>(),
                                   std::memory_order_release);
    }
};

inline NativeDamageClassStatRegistry& nativeDamageClassStats() {
    static NativeDamageClassStatRegistry registry;
    return registry;
}

}
