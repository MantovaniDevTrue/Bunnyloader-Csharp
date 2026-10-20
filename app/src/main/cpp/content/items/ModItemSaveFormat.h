#pragma once

#include <charconv>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace bl::runtime::item_save {

struct SavedItem {
    std::string container;
    int slot = 0;
    int stack = 1;
    int prefix = 0;
    bool favorited = false;
    std::string key;
    std::string prefixKey{};
    std::string data{};
    std::string rarityKey{};
};

inline bool hasSavedVanillaPrefix(const SavedItem& item) {
    return item.prefix > 0 || !item.prefixKey.empty();
}

inline SavedItem remainingVanillaMetadata(const SavedItem& saved,
    int currentPrefix, const std::string& currentRarity) {
    SavedItem remaining = saved;
    if (currentPrefix != 0) {
        remaining.prefix = 0;
        remaining.prefixKey.clear();
    }
    if (!currentRarity.empty()) remaining.rarityKey.clear();
    return remaining;
}

inline bool hasRemainingVanillaMetadata(const SavedItem& saved) {
    return hasSavedVanillaPrefix(saved) || !saved.rarityKey.empty();
}

inline bool mergePending(std::vector<SavedItem>& saved, const SavedItem& pending) {
    for (auto& current : saved) {
        if (current.container != pending.container ||
            current.slot != pending.slot || current.key != pending.key) continue;
        if (current.prefix == 0 && current.prefixKey.empty())
            current.prefixKey = pending.prefixKey;
        if (current.rarityKey.empty())
            current.rarityKey = pending.rarityKey;
        return true;
    }
    return false;
}

inline bool integer(const std::string& text, int& value) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc() && result.ptr == text.data() + text.size();
}

inline bool validRarityKey(const std::string& key) {
    if (key.empty()) return true;
    const size_t slash = key.find('/');
    if (slash == std::string::npos || slash == 0 || slash > 96 ||
        key.size() <= slash + 1 || key.size() > slash + 73 ||
        key.find('/', slash + 1) != std::string::npos) return false;
    for (size_t i = 0; i < key.size(); ++i) {
        if (i == slash) continue;
        const unsigned char c = static_cast<unsigned char>(key[i]);
        const bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        const bool digit = c >= '0' && c <= '9';
        if (!letter && !digit && c != '_' &&
            (i > slash || (c != '-' && c != '.'))) return false;
    }
    return true;
}

inline bool decode(const std::string& line, SavedItem& out, bool allowRarity = true) {
    std::vector<std::string> cols;
    size_t start = 0;

    for (size_t tab; (tab = line.find('\t', start)) != std::string::npos; start = tab + 1) {
        cols.push_back(line.substr(start, tab - start));
    }

    cols.push_back(line.substr(start));
    if ((cols.size() != 6 && cols.size() != 7 &&
        !(allowRarity && cols.size() == 8)) ||
        cols[0].empty() || cols[0] == "trash" || cols[5].empty()) return false;

    SavedItem value;
    value.container = cols[0];
    if (!integer(cols[1], value.slot) || value.slot < 0 || !integer(cols[2], value.stack) || value.stack <= 0) return false;
    if (cols[3].find('/') != std::string::npos) value.prefixKey = cols[3];
    else if (!integer(cols[3], value.prefix) || value.prefix < 0 || value.prefix > 255) return false;
    if (cols[4] != "0" && cols[4] != "1") return false;

    value.favorited = cols[4] == "1";
    value.key = cols[5];
    if (cols.size() >= 7) value.data = cols[6];
    if (cols.size() == 8) {
        if (!validRarityKey(cols[7])) return false;
        value.rarityKey = cols[7];
    }

    out = std::move(value);

    return true;
}

inline bool read(std::istream& stream, std::vector<SavedItem>& out, bool strict = false) {
    std::string line;
    if (!std::getline(stream, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const bool version3 = line == "bunnyloader itens 3";
    if (!version3 && line != "bunnyloader itens 1" && line != "bunnyloader itens 2") return false;
    if (version3 && stream.eof()) return false;

    std::vector<SavedItem> records;

    while (std::getline(stream, line)) {
        if (version3 && stream.eof()) return false;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (version3) {
            size_t delimiters = 0;
            for (char ch : line)
                if (ch == '\t') ++delimiters;
            if (delimiters != 7) return false;
        }
        SavedItem value;
        if (decode(line, value, version3)) {
            records.push_back(std::move(value));
        } else if (version3 || strict) {
            return false;
        }
    }

    if (stream.bad() || (version3 && records.empty())) return false;

    out = std::move(records);

    return true;
}

inline bool write(std::ostream& stream, const std::vector<SavedItem>& items) {
    bool version3 = false;
    for (const auto& item : items)
        if (!item.rarityKey.empty()) version3 = true;
    stream << (version3 ? "bunnyloader itens 3\n" : "bunnyloader itens 2\n");

    for (const auto& item : items) {
        // JSON escapes tab/newline/NUL. Reject unescaped delimiters rather than
        // allowing a callback or a damaged file to inject another record.
        for (const auto* text : {&item.container, &item.key, &item.prefixKey, &item.data, &item.rarityKey}) {
            if (text->find_first_of("\t\r\n") != std::string::npos || text->find('\0') != std::string::npos) return false;
        }

        if (!validRarityKey(item.rarityKey)) return false;
        stream << item.container << '\t' << item.slot << '\t' << item.stack << '\t'
               << (item.prefixKey.empty() ? std::to_string(item.prefix) : item.prefixKey) << '\t'
               << (item.favorited ? 1 : 0) << '\t' << item.key << '\t' << item.data;
        if (version3) stream << '\t' << item.rarityKey;
        stream << '\n';
    }

    return static_cast<bool>(stream);
}

} // namespace bl::runtime::item_save
