#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cctype>
#include <map>
#include <mutex>
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

class Registry {
public:
    bool publish(const std::string& mod, const std::vector<Quest>& quests, std::string* error) {
        if (mod.empty() || quests.size() > 4096) return fail(error, "InvalidModOrCapacity");
        std::lock_guard<std::mutex> lock(mutex_);
        std::unordered_set<int> stagedTypes;
        std::unordered_set<std::string> stagedNames;
        for (const auto& quest : quests) {
            if (quest.mod != mod || !validKey(quest.name) || quest.type < 0 || quest.type >= 65536 ||
                (!quest.preHardmode && !quest.hardmode) ||
                (quest.catchRules & ~3) != 0 ||
                !validText(quest.description) || !validText(quest.location))
                return fail(error, "InvalidQuestDescriptor");
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

    std::vector<Quest> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return registered_;
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

    static std::string chat(const Quest& quest, const std::string& fallbackName) {
        const std::string& description = quest.description.empty() ? fallbackName : quest.description;
        return quest.location.empty() ? description : description + "\n\n(" + quest.location + ")";
    }

    template<typename Swap, typename ReadType, typename Finished>
    static int reroll(Swap&& swap, ReadType&& current, Finished&& finished,
                      const std::vector<Quest>& quests, bool hardmode) {
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

private:
    static bool fail(std::string* error, const char* detail) {
        if (error) *error = detail;
        return false;
    }

    mutable std::mutex mutex_;
    std::vector<Quest> registered_;
    std::atomic<uint64_t> generation_{0};
};

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