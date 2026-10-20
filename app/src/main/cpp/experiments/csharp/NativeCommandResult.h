#pragma once

#include "experiments/csharp/NativeCommandMailbox.h"

#include <charconv>
#include <sstream>
#include <string>
#include <vector>

namespace bl::csharp {

// Decodificador sem alocacoes proporcionais a entradas arbitrarias.
// O destino e obtido do ticket nativo, nunca de texto controlado pelo mod.
inline bool decodeNativeCommandResult(const std::string& wire, int playerIndex,
                                      bool* handled, std::vector<NativeCommandReply>* replies,
                                      std::string* error = nullptr,
                                      NativeCommandAction* action = nullptr) {
    auto fail = [&](const char* reason) {
        if (error) *error = reason;
        return false;
    };
    if (!handled || !replies || playerIndex < -1 || playerIndex > 255 ||
        wire.empty() || wire.size() > 20 * 1024 || wire.back() != '\n')
        return fail("CommandResultInvalidSize");
    replies->clear();
    if (action) *action = {};
    bool receivedAction = false;
    std::istringstream input(wire);
    std::string line;
    if (!std::getline(input, line) || line != "BUNNY_COMMAND_RESULT_V1")
        return fail("CommandResultHeader");
    if (!std::getline(input, line) || (line != "HANDLED\t0" && line != "HANDLED\t1"))
        return fail("CommandResultHandled");
    *handled = line == "HANDLED\t1";
    auto number = [](const std::string& text, uint8_t* result) {
        if (text.empty() || text.size() > 3) return false;
        unsigned n = 0;
        for (char c : text) {
            if (c < '0' || c > '9') return false;
            n = n * 10 + static_cast<unsigned>(c - '0');
            if (n > 255) return false;
        }
        *result = static_cast<uint8_t>(n);
        return true;
    };
    while (std::getline(input, line)) {
        if (!*handled || replies->size() >= NativeCommandMailbox::MaxReplyLines)
            return fail("CommandResultUnexpectedReply");
        if (line.starts_with("ACTION\t")) {
            if (!action || receivedAction || !replies->empty())
                return fail("CommandResultMultipleActions");
            const auto separator = line.find('\t', 7);
            if (separator == std::string::npos ||
                line.find('\t', separator + 1) != std::string::npos)
                return fail("CommandResultInvalidAction");
            const std::string kind = line.substr(7, separator - 7);
            const std::string amount = line.substr(separator + 1);
            int number = 0;
            const auto parsed = std::from_chars(amount.data(), amount.data() + amount.size(), number);
            if (parsed.ec != std::errc{} ||
                parsed.ptr != amount.data() + amount.size())
                return fail("CommandResultInvalidActionAmount");
            if (kind == "HEAL" && number > 0 && number <= 1000000)
                action->kind = NativeCommandActionKind::Heal;
            else if (kind == "ADDTIME" && number >= -86400000 && number <= 86400000)
                action->kind = NativeCommandActionKind::AdvanceWorldTime;
            else
                return fail("CommandResultInvalidActionKindOrAmount");
            action->amount = number;
            receivedAction = true;
            continue;
        }
        if (receivedAction) return fail("CommandResultReplyAfterAction");
        std::istringstream record(line);
        std::string prefix, red, green, blue, encoded;
        if (!std::getline(record, prefix, '\t') ||
            !std::getline(record, red, '\t') ||
            !std::getline(record, green, '\t') ||
            !std::getline(record, blue, '\t') || record.eof())
            return fail("CommandResultFields");
        std::getline(record, encoded);
        if (prefix != "REPLY" || encoded.find('\t') != std::string::npos ||
            encoded.size() > NativeCommandMailbox::MaxLineBytes * 2 ||
            encoded.size() % 2 != 0)
            return fail("CommandResultFields");
        NativeCommandReply reply;
        reply.playerIndex = playerIndex;
        if (!number(red, &reply.red) || !number(green, &reply.green) ||
            !number(blue, &reply.blue))
            return fail("CommandResultColors");
        auto hex = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        reply.text.reserve(encoded.size() / 2);
        for (size_t i = 0; i < encoded.size(); i += 2) {
            const int hi = hex(encoded[i]), lo = hex(encoded[i + 1]);
            if (hi < 0 || lo < 0)
                return fail("CommandResultHex");
            reply.text.push_back(static_cast<char>((hi << 4) | lo));
        }
        replies->push_back(std::move(reply));
    }
    return true;
}

} // namespace bl::csharp
