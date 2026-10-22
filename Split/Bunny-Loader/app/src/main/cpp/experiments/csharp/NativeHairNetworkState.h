#pragma once

#include "experiments/csharp/NativeHairRegistry.h"

#include <charconv>
#include <map>
#include <string>
#include <string_view>

namespace bl::csharp::hair {

// O transporte nativo enviará esses payloads após a mensagem vanilla 4
// (aparência do jogador). Nunca transmitir o ID numérico de um ModHair,
// porque sua posição difere conforme a ordem dos mods dos dois clientes.
//
// H1|7|modUid/ClassName -> cabelo de mod, resolvido localmente
// H1|7|#12              -> cabelo vanilla 12, inclusive remoção de mod
//
// O servidor só aceita player==sender (0..255); clientes apenas do servidor
// (sender=256). O próprio NetManager/NetPacket ainda precisa ser conectado.
struct NetworkUpdate {
    bool accepted = false;
    bool changed = false;
    bool fallback = false;
    bool relay = false;
    int player = -1;
    int localType = 0;
    int ignoreClient = -1;
    std::string stableIdentity;
    std::string relayPacket;
};

class NetworkState {
public:
    explicit NetworkState(const Registry& registry) : registry_(registry) {}

    std::string EncodeOutgoing(int player, int localHairType) const {
        if (player < 0 || player > 255) return {};
        if (localHairType >= 0 && localHairType < Registry::kVanillaCount)
            return "H1|" + std::to_string(player) + "|#" +
                   std::to_string(localHairType);
        return registry_.EncodeAppearance(player, localHairType);
    }

    bool Receive(std::string_view packet, Receiver receiver,
                 int from, NetworkUpdate* result) {
        if (!result) return false;
        *result = {};
        if (!packet.starts_with("H1|") || packet.size() > 300) return false;

        const size_t indexEnd = packet.find('|', 3);
        if (indexEnd == std::string_view::npos) return false;
        const std::string_view number = packet.substr(3, indexEnd - 3);
        int player = -1;
        if (!numberInt(number, 0, 255, &player)) return false;
        if (receiver == Receiver::Server) {
            if (from < 0 || from > 255 || from != player) return false;
        } else if (from != 256) {
            return false;
        }

        const auto name = packet.substr(indexEnd + 1);
        int localType = 0;
        bool fallback = false;
        if (name.starts_with("#")) {
            const auto id = name.substr(1);
            if (!numberInt(id, 0, Registry::kVanillaCount - 1, &localType))
                return false;
        } else {
            Appearance appearance;
            if (!registry_.DecodeAppearance(packet, receiver, from, &appearance))
                return false;
            localType = appearance.type;
            fallback = appearance.fallback;
        }

        const std::string stable(name);
        auto it = selected_.find(player);
        const bool changed = it == selected_.end() || it->second != stable;
        if (changed) selected_[player] = stable;

        result->accepted = true;
        result->changed = changed;
        result->fallback = fallback;
        result->relay = changed && receiver == Receiver::Server;
        result->player = player;
        result->localType = localType;
        result->ignoreClient = receiver == Receiver::Server ? from : -1;
        result->stableIdentity = stable;
        if (result->relay) result->relayPacket = std::string(packet);
        return true;
    }

    // Chamado quando um cliente abandona o mundo ou seu slot é reutilizado.
    // Uma atualização antiga nunca deve impedir o novo jogador de sincronizar.
    void ForgetPlayer(int player) { selected_.erase(player); }
    void Reset() { selected_.clear(); }
    size_t TrackedPlayers() const { return selected_.size(); }

private:
    static bool numberInt(std::string_view text, int lower, int upper,
                          int* result) {
        if (!result || text.empty() || text.size() > 3 ||
            (text.size() > 1 && text.front() == '0')) return false;
        int value = -1;
        const auto parsed = std::from_chars(text.data(),
                                             text.data() + text.size(), value);
        if (parsed.ec != std::errc{} ||
            parsed.ptr != text.data() + text.size() ||
            value < lower || value > upper) return false;
        *result = value;
        return true;
    }

    const Registry& registry_;
    std::map<int, std::string> selected_;
};

} // namespace bl::csharp::hair
