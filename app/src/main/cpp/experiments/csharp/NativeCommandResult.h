#pragma once

#include "experiments/csharp/NativeCommandMailbox.h"

#include <sstream>
#include <string>
#include <vector>

namespace bl::csharp {

// Decodificador sem alocacoes proporcionais a entradas arbitrarias.
// O destino e obtido do ticket nativo, nunca de texto controlado pelo mod.
inline bool decodeNativeCommandResult(const std::string& wire, int playerIndex,
                                      bool* handled, std::vector<NativeCommandReply>* replies,
                                      std::string* error = nullptr) {
    auto fail = [&](const char* reason) {
        if (error) *error = reason;
        return false;
    };
    if (!handled || !replies || playerIndex < -1 || playerIndex > 255 ||
        wire.empty() || wire.size() > 20 * 1024 || wire.back() != '\n')
        return fail("CommandResultInvalidSize");
    replies->clear();
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
        std::istringstream record(line);
        std::string prefix, red, green, blue, encoded, extra;
        if (!std::getline(record, prefix, '\t') ||
            !std::getline(record, red, '\t') ||
            !std::getline(record, green, '\t') ||
            !std::getline(record, blue, '\t') ||
            !std::getline(record, encoded, '\t') ||
            std::getline(record, extra, '\t') || prefix != "REPLY" ||
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
