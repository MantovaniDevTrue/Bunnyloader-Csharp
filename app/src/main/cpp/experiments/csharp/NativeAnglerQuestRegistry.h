#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cctype>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace bl::csharp::angler {

struct Quest {
    std::string mod;
    std::string name;
    int type = -1;
    bool preHardmode = true;
    bool hardmode = true;
    std::string description;
    std::string location;
    int catchRules = 0;
    std::map<std::string, std::pair<std::string, std::string>> locales{};
    bool catchEnabled = false;
    int callbackMask = 0;
    bool operator==(const Quest&) const = default;
};

inline bool validKey(const std::string& key) {
    if (key.empty() || key.size() > 72) return false;
    return std::all_of(key.begin(), key.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_';
    });
}

inline bool validText(const std::string& text) {
    if (text.size() > 512) return false;
    for (size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c == 0 || c == '\r' || c == '\t' || c == 127) return false;
        if (c < 0x80) { ++i; continue; }
        unsigned length = 0;
        uint32_t codepoint = 0;
        if (c >= 0xC2 && c <= 0xDF) { length = 2; codepoint = c & 0x1F; }
        else if (c >= 0xE0 && c <= 0xEF) { length = 3; codepoint = c & 0x0F; }
        else if (c >= 0xF0 && c <= 0xF4) { length = 4; codepoint = c & 0x07; }
        else return false;
        if (length > text.size() - i) return false;
        for (unsigned k = 1; k < length; ++k) {
            auto tail = static_cast<unsigned char>(text[i + k]);
            if ((tail & 0xC0) != 0x80) return false;
            codepoint = (codepoint << 6) | (tail & 0x3F);
        }
        if ((length == 2 && codepoint < 0x80) ||
            (length == 3 && codepoint < 0x800) ||
            (length == 4 && codepoint < 0x10000) ||
            (codepoint >= 0xD800 && codepoint <= 0xDFFF) ||
            codepoint > 0x10FFFF) return false;
        i += length;
    }
    return true;
}

inline bool validCulture(const std::string& culture) {
    if (culture.size() < 2 || culture.size() > 20) return false;
    return std::all_of(culture.begin(), culture.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '-';
    });
}

struct QuestListUpdate {
    std::vector<int> ids;
    int selected = 0;
    bool changed = false;
};

class Registry {
public:
    static QuestListUpdate reconcileIds(const std::vector<int>& original,
                                        const std::set<int>& formerlyOwned,
                                        const std::vector<Quest>& quests,
                                        int selected) {
        QuestListUpdate result;
        result.ids.reserve(original.size() + quests.size());
        std::set<int> existing;
        int selectedType = selected >= 0 && static_cast<size_t>(selected) < original.size()
            ? original[static_cast<size_t>(selected)] : -1;
        std::set<int> activeCSharp;
        for (const auto& quest : quests) activeCSharp.insert(quest.type);
        for (int id : original) {
            if (formerlyOwned.contains(id)) continue;
            if (existing.insert(id).second || !activeCSharp.contains(id))
                result.ids.push_back(id);
        }
        std::vector<const Quest*> ordered;
        ordered.reserve(quests.size());
        for (const auto& quest : quests) ordered.push_back(&quest);
        std::sort(ordered.begin(), ordered.end(),
            [](const Quest* left, const Quest* right) {
                if (left->mod != right->mod) return left->mod < right->mod;
                if (left->name != right->name) return left->name < right->name;
                return left->type < right->type;
            });
        for (const Quest* quest : ordered)
            if (existing.insert(quest->type).second)
                result.ids.push_back(quest->type);
        result.changed = result.ids != original;
        auto it = std::find(result.ids.begin(), result.ids.end(), selectedType);
        result.selected = it == result.ids.end() ? 0 :
            static_cast<int>(it - result.ids.begin());
        return result;
    }

    static uint64_t manifestFingerprint(const std::vector<Quest>& quests) {
        std::vector<const Quest*> ordered;
        ordered.reserve(quests.size());
        for (const auto& quest : quests) ordered.push_back(&quest);
        std::sort(ordered.begin(), ordered.end(),
            [](const Quest* a, const Quest* b) {
                if (a->mod != b->mod) return a->mod < b->mod;
                if (a->name != b->name) return a->name < b->name;
                return a->type < b->type;
            });
        uint64_t hash = 14695981039346656037ull;
        const auto addByte = [&](uint8_t byte) {
            hash ^= byte;
            hash *= 1099511628211ull;
        };
        const auto addString = [&](const std::string& value) {
            for (unsigned char ch : value) addByte(ch);
            addByte(0);
        };
        for (const Quest* quest : ordered) {
            addString(quest->mod);
            addString(quest->name);
            const auto id = static_cast<uint32_t>(quest->type);
            for (unsigned shift = 0; shift != 32; shift += 8)
                addByte(static_cast<uint8_t>((id >> shift) & 0xffu));
            addByte(quest->preHardmode ? 1 : 0);
            addByte(quest->hardmode ? 1 : 0);
            addByte(static_cast<uint8_t>(quest->catchRules));
            addByte(static_cast<uint8_t>(quest->callbackMask));
            addByte(quest->catchEnabled ? 1 : 0);
            addString(quest->description);
            addString(quest->location);
            for (const auto& [culture, text] : quest->locales) {
                addString(culture);
                addString(text.first);
                addString(text.second);
            }
            addByte(0xffu);
        }
        return hash;
    }

    bool publish(const std::string& mod, const std::vector<Quest>& quests, std::string* error) {
        if (mod.empty() || quests.size() > 4096) return fail(error, "InvalidModOrCapacity");
        std::lock_guard<std::mutex> lock(mutex_);
        std::unordered_set<int> stagedTypes;
        std::unordered_set<std::string> stagedNames;
        for (const auto& quest : quests) {
            if (quest.mod != mod || !validKey(quest.name) || quest.type < 0 || quest.type >= 65536 ||
                (!quest.preHardmode && !quest.hardmode) ||
                (quest.catchRules & ~3) != 0 ||
                (quest.callbackMask & ~3) != 0 ||
                !validText(quest.description) || !validText(quest.location) ||
                quest.locales.size() > 8)
                return fail(error, "InvalidQuestDescriptor");
            for (const auto& [culture, chat] : quest.locales)
                if (!validCulture(culture) || !validText(chat.first) ||
                    !validText(chat.second) ||
                    (chat.first.empty() && chat.second.empty()))
                    return fail(error, "InvalidQuestTranslation");
            if (!stagedTypes.insert(quest.type).second ||
                !stagedNames.insert(quest.name).second)
                return fail(error, "DuplicateQuestInMod");
            for (const auto& existing : registered_) {
                if (existing.mod != mod &&
                    (existing.type == quest.type ||
                     (existing.mod == quest.mod && existing.name == quest.name)))
                    return fail(error, "QuestTypeOwnedByAnotherMod");
            }
        }
        std::vector<Quest> next;
        next.reserve(registered_.size() + quests.size());
        for (const auto& old : registered_)
            if (old.mod != mod) next.push_back(old);
        next.insert(next.end(), quests.begin(), quests.end());
        std::sort(next.begin(), next.end(), [](const Quest& left, const Quest& right) {
            if (left.mod != right.mod) return left.mod < right.mod;
            if (left.name != right.name) return left.name < right.name;
            return left.type < right.type;
        });
        if (next == registered_) return true;
        registered_.swap(next);
        generation_.fetch_add(1, std::memory_order_release);
        return true;
    }

    void unload(const std::string& mod) {
        std::lock_guard<std::mutex> lock(mutex_);
        const size_t before = registered_.size();
        registered_.erase(std::remove_if(registered_.begin(), registered_.end(),
            [&](const Quest& quest) { return quest.mod == mod; }), registered_.end());
        if (registered_.size() != before)
            generation_.fetch_add(1, std::memory_order_release);
    }

    uint64_t generation() const { return generation_.load(std::memory_order_acquire); }

    std::pair<uint64_t, std::vector<Quest>> snapshotVersion() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {generation_.load(std::memory_order_relaxed), registered_};
    }

    std::vector<Quest> snapshot() const {
        return snapshotVersion().second;
    }

    static std::vector<int> mergeIds(const std::vector<int>& original,
                                     const std::vector<Quest>& quests) {
        std::vector<int> result = original;
        std::unordered_set<int> seen(original.begin(), original.end());
        for (const auto& quest : quests)
            if (seen.insert(quest.type).second) result.push_back(quest.type);
        return result;
    }

    static bool available(const Quest& quest, bool hardmode) {
        return hardmode ? quest.hardmode : quest.preHardmode;
    }
    static bool canCatch(const Quest& quest, bool invertedGravity, bool uncommon) {
        return ((quest.catchRules & 1) == 0 || invertedGravity) &&
               ((quest.catchRules & 2) == 0 || uncommon);
    }

    static std::string chat(const Quest& quest, const std::string& fallbackName,
                            const std::string& culture = "en-US") {
        std::string description = quest.description.empty() ? fallbackName : quest.description;
        std::string location = quest.location;
        auto chosen = quest.locales.find(culture);
        if (chosen == quest.locales.end()) {
            const auto prefix = culture.substr(0, culture.find('-'));
            for (auto it = quest.locales.begin(); it != quest.locales.end(); ++it)
                if (it->first.substr(0, it->first.find('-')) == prefix) {
                    chosen = it;
                    break;
                }
        }
        if (chosen != quest.locales.end()) {
            if (!chosen->second.first.empty()) description = chosen->second.first;
            if (!chosen->second.second.empty()) location = chosen->second.second;
        }
        return location.empty() ? description : description + "\n\n(" + location + ")";
    }

    template<typename Swap, typename ReadType, typename Finished, typename Available>
    static int reroll(Swap&& swap, ReadType&& current, Finished&& finished,
                      const std::vector<Quest>& quests, bool hardmode, Available&& available) {
        for (int retries = 0; retries < 50; ++retries) {
            if (finished()) break;
            const int type = current();
            const auto quest = std::find_if(quests.begin(), quests.end(),
                [&](const Quest& q) { return q.type == type; });
            if (quest == quests.end() || available(*quest, hardmode)) break;
            swap();
            if (retries == 49) return 50;
        }
        return 0;
    }

    template<typename Swap, typename ReadType, typename Finished>
    static int reroll(Swap&& swap, ReadType&& current, Finished&& finished,
                      const std::vector<Quest>& quests, bool hardmode) {
        return reroll(std::forward<Swap>(swap),std::forward<ReadType>(current),
                      std::forward<Finished>(finished),quests,hardmode,
                      [](const Quest& quest,bool mode) { return Registry::available(quest,mode); });
    }

private:
    static bool fail(std::string* error, const char* detail) {
        if (error) *error = detail;
        return false;
    }

    mutable std::mutex mutex_;
    std::vector<Quest> registered_;
    std::atomic<uint64_t> generation_{0};
};

inline std::atomic<uint64_t>& activeGeneration() {
    static std::atomic<uint64_t> generation{0};
    return generation;
}

inline Registry& registry() {
    static Registry value;
    return value;
}

inline bool decodeHex(const std::string& encoded, std::string* decoded) {
    if (!decoded) return false;
    if (encoded == "-") { decoded->clear(); return true; }
    if (encoded.size() > 1024 || encoded.size() % 2 != 0) return false;
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string result;
    result.reserve(encoded.size() / 2);
    for (size_t index = 0; index < encoded.size(); index += 2) {
        const int hi = nibble(encoded[index]), lo = nibble(encoded[index + 1]);
        if (hi < 0 || lo < 0) return false;
        result.push_back(static_cast<char>(hi * 16 + lo));
    }
    if (!validText(result)) return false;
    *decoded = std::move(result);
    return true;
}

}