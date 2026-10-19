#pragma once
#include "experiments/csharp/NativeLocalizationSession.h"
#include <set>
#include <string>
#include <string_view>

namespace bl::csharp::localization {

inline bool validOwnedLocaleKey(std::string_view key) {
    if (key.size() < 8 || key.size() > 280 || key.substr(0, 5) != "Mods.") return false;
    const size_t idEnd = key.find('.', 5);
    if (idEnd == std::string_view::npos || idEnd == 5 || idEnd + 1 >= key.size()) return false;
    const auto safeChar=[](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') || c == '-' || c == '_';
    };
    for (size_t i = 5; i < idEnd; ++i)
        if (!safeChar(key[i])) return false;
    for (size_t i = idEnd + 1; i < key.size(); ++i) {
        char c = key[i];
        if (!safeChar(c) && c != '.' && c != '/' && c != '$') return false;
        if (c == '.' && key[i - 1] == '.') return false;
    }
    const size_t variant=key.find('$',idEnd+1);
    if(variant!=std::string_view::npos &&
       (variant==idEnd+1 || variant==key.size()-1 ||
        key[variant-1]=='.' || key[variant-1]=='/' ||
        key.find('$',variant+1)!=std::string_view::npos ||
        key.find_first_of("./",variant+1)!=std::string_view::npos))return false;
    return key.back() != '.';
}

inline bool safeToRetire(const SessionSnapshot& state, const std::string& key) {
    if (!validOwnedLocaleKey(key) || !state.retired.contains(key)) return false;
    for (const auto& [id,entries]:state.active)
        for (const auto& entry:entries)
            if (entry.key == key) return false;
    return true;
}

inline bool safeToRetireAll(const SessionSnapshot& state,
                            const std::set<std::string>& createdExternalKeys) {
    std::set<std::string> active;
    for (const auto& [id,entries] : state.active)
        for (const auto& entry : entries) active.insert(entry.key);
    for (const auto& key : state.retired) {
        const bool isCreatedExternal = createdExternalKeys.contains(key) &&
            key.rfind("Mods.", 0) != 0 && key.find('.') != std::string::npos;
        if ((!validOwnedLocaleKey(key) && !isCreatedExternal) ||
            active.contains(key)) return false;
    }
    return true;
}

inline bool safeToRetireAll(const SessionSnapshot& state) {
    std::set<std::string> active;
    for (const auto& [id,entries]:state.active)
        for (const auto& entry:entries)
            active.insert(entry.key);
    for (const auto& key:state.retired)
        if (!validOwnedLocaleKey(key) || active.contains(key)) return false;
    return true;
}
}
