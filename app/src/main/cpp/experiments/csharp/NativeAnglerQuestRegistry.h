#pragma once

#include <algorithm>
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
};

inline bool validKey(const std::string& key) {
    if (key.empty() || key.size() > 72) return false;
    return std::all_of(key.begin(), key.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_';
    });
}

inline bool validText(const std::string& text) {
    if (text.size() > 512) return false;
    return std::none_of(text.begin(), text.end(), [](unsigned char c) {
        return c == 0 || c == '\r' || c == '\t' || c == 127;
    });
}

class Registry {
public:
    bool publish(const std::string& mod, const std::vector<Quest>& quests, std::string* error) {
        if (mod.empty() || quests.size() > 4096) return fail(error, "InvalidModOrCapacity");
        std::lock_guard<std::mutex> lock(mutex_);
        std::unordered_set<int> stagedTypes;
        std::unordered_set<std::string> stagedNames;
        for (const auto& quest : quests) {
            if (quest.mod != mod || !validKey(quest.name) || quest.type < 0 ||
                (!quest.preHardmode && !quest.hardmode) ||
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
        return true;
    }

    void unload(const std::string& mod) {
        std::lock_guard<std::mutex> lock(mutex_);
        registered_.erase(std::remove_if(registered_.begin(), registered_.end(),
            [&](const Quest& quest) { return quest.mod == mod; }), registered_.end());
    }

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