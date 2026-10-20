#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace bl::csharp {

// Os valores devem coincidir com BunnyCommandType no SDK C#.
enum class NativeCommandRoute : uint8_t {
    Chat = 1,
    Server = 2,
    World = 4
};

struct NativeCommandSpec {
    std::string name;
    uint8_t routes = 1;
};

struct NativeCommandRequest {
    uint64_t epoch = 0;
    uint64_t sequence = 0;
    std::string input;
    int playerIndex = -1;
    int networkMode = 0;
    NativeCommandRoute caller = NativeCommandRoute::Chat;
};

enum class NativeCommandActionKind : uint8_t {
    Heal = 1,
    AdvanceWorldTime = 2
};

struct NativeCommandAction {
    NativeCommandActionKind kind = NativeCommandActionKind::Heal;
    int amount = 0;
    uint64_t epoch = 0;
    int playerIndex = -1;
    int networkMode = 0;
    NativeCommandRoute caller = NativeCommandRoute::Chat;
};

struct NativeCommandReply {
    std::string text;
    int playerIndex = -1;
    uint8_t red = 255;
    uint8_t green = 240;
    uint8_t blue = 20;
};

enum class NativeCommandSubmit : uint8_t {
    Vanilla = 0,
    Queued = 1,
    Busy = 2
};

// O chat do Terraria usa a thread do jogo. O callback C# usa a thread Mono.
// Esta fila entrega COPIAS imutaveis; nenhum objeto IL2CPP cruza as threads.
// Os hooks e o dispatcher Mono precisam usar a mesma epoca de mundo.
class NativeCommandMailbox final {
public:
    static constexpr size_t MaxPending = 64;
    static constexpr size_t MaxReplies = 128;
    static constexpr size_t MaxInputBytes = 1024;
    static constexpr size_t MaxLineBytes = 512;
    static constexpr size_t MaxReplyLines = 16;
    static constexpr size_t MaxRegisteredMods = 128;
    static constexpr size_t MaxAliasesPerMod = 128 * 17;
    static constexpr size_t MaxRegisteredNames = 8192;

    bool registerMod(const std::string& modId, const std::vector<NativeCommandSpec>& specs,
                     std::string* error = nullptr, bool ready = true) {
        const std::string ownerId = normalize(modId);
        if (ownerId.empty() || specs.empty() || specs.size() > MaxAliasesPerMod) {
            if (error) *error = "InvalidOwnerOrSpecCount";
            return false;
        }
        std::vector<Registered> proposal;
        proposal.reserve(specs.size());
        std::set<std::string> distinct;
        for (const auto& spec : specs) {
            const std::string name = normalize(spec.name);
            if (name.empty() || !distinct.insert(name).second ||
                spec.routes == 0 || (spec.routes & ~uint8_t{7}) != 0) {
                if (error) *error = "InvalidOrDuplicateCommandName";
                return false;
            }
            proposal.push_back({ownerId, name, spec.routes});
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (owners_.contains(ownerId) || owners_.size() >= MaxRegisteredMods ||
            registeredNames_ + proposal.size() > MaxRegisteredNames) {
            if (error) *error = "OwnerAlreadyRegisteredOrCapacityReached";
            return false;
        }
        owners_.emplace(ownerId, proposal);
        registeredNames_ += proposal.size();
        if (ready) readyOwners_.insert(ownerId);
        for (const auto& spec : proposal)
            names_[spec.name].push_back(spec);
        return true;
    }

    // O compilador pode descobrir um comando antes de o worker Mono abrir a
    // DLL. Nomes pendentes nao devem consumir mensagens do chat vanilla.
    bool setModReady(const std::string& modId, bool ready) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string ownerId = normalize(modId);
        if (!owners_.contains(ownerId)) return false;
        if (ready) readyOwners_.insert(ownerId);
        else readyOwners_.erase(ownerId);
        return true;
    }

    bool unregisterMod(const std::string& modId) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string ownerId = normalize(modId);
        const auto owner = owners_.find(ownerId);
        if (owner == owners_.end()) return false;
        for (const auto& spec : owner->second) {
            auto it = names_.find(spec.name);
            if (it == names_.end()) continue;
            auto& list = it->second;
            std::erase_if(list, [&](const Registered& item) { return item.owner == ownerId; });
            if (list.empty()) names_.erase(it);
        }
        registeredNames_ -= owner->second.size();
        readyOwners_.erase(ownerId);
        owners_.erase(owner);
        // Um callback ja entregue pode estar executando no MonoVM. A epoca nova
        // invalida sua resposta e todos os pedidos ainda em transito.
        invalidateLocked();
        return true;
    }

    void newWorld(bool clearCommands = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        invalidateLocked();
        if (clearCommands) {
            owners_.clear();
            names_.clear();
            registeredNames_ = 0;
            readyOwners_.clear();
        }
    }

    NativeCommandSubmit submit(const std::string& input, int playerIndex,
                                int netMode, NativeCommandRoute caller) {
        if (input.empty() || input.size() > MaxInputBytes ||
            playerIndex < -1 || playerIndex > 255 || netMode < 0 || netMode > 3 ||
            (caller != NativeCommandRoute::Chat && caller != NativeCommandRoute::Server))
            return NativeCommandSubmit::Vanilla;
        const size_t start = input.find_first_not_of(" \t\r\n");
        if (start == std::string::npos || input[start] != '/')
            return NativeCommandSubmit::Vanilla;
        const std::string text = input.substr(start);

        size_t end = text.find_first_of(" \t\r\n");
        std::string token = text.substr(1, end == std::string::npos ? end : end - 1);
        const size_t separator = token.find(':');
        std::string owner;
        if (separator != std::string::npos) {
            owner = normalize(token.substr(0, separator));
            token = token.substr(separator + 1);
            if (owner.empty()) return NativeCommandSubmit::Vanilla;
        }
        const std::string name = normalize(token);
        if (name.empty()) return NativeCommandSubmit::Vanilla;

        std::lock_guard<std::mutex> lock(mutex_);
        const auto found = names_.find(name);
        if (found == names_.end()) return NativeCommandSubmit::Vanilla;
        bool recognized = false;
        for (const auto& candidate : found->second) {
            if (readyOwners_.contains(candidate.owner) &&
                matches(candidate.routes, caller, netMode)) {
                // Com nome de mod desconhecido, o dispatcher C# respondera
                // corretamente qualificado, como no CommandLoader original.
                recognized = true;
                break;
            }
        }
        if (!recognized) return NativeCommandSubmit::Vanilla;
        if (inFlight_.size() >= MaxPending)
            return NativeCommandSubmit::Busy;
        const uint64_t ticket = ++sequence_;
        inFlight_.emplace(ticket, Ticket{playerIndex, netMode, caller});
        requests_.push_back({epoch_, ticket, text, playerIndex, netMode, caller});
        return NativeCommandSubmit::Queued;
    }

    // Chamado somente do worker Mono. Nunca manter o mutex durante Action().
    std::vector<NativeCommandRequest> takeRequests(size_t budget = 16) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<NativeCommandRequest> out;
        budget = std::min(budget, MaxPending);
        out.reserve(std::min(budget, requests_.size()));
        while (budget-- && !requests_.empty()) {
            out.push_back(std::move(requests_.front()));
            requests_.pop_front();
        }
        return out;
    }

    bool deliver(uint64_t epoch, uint64_t sequence,
                 const std::vector<NativeCommandReply>& messages) {
        if (messages.size() > MaxReplyLines) return false;
        for (const auto& message : messages)
            if (message.text.size() > MaxLineBytes ||
                message.playerIndex < -1 || message.playerIndex > 255)
                return false;

        std::lock_guard<std::mutex> lock(mutex_);
        const auto pending = inFlight_.find(sequence);
        if (epoch != epoch_ || pending == inFlight_.end() ||
            replies_.size() + messages.size() > MaxReplies)
            return false;
        // Uma resposta nao pode redirecionar texto privado a outro jogador.
        for (const auto& message : messages)
            if (message.playerIndex != pending->second.playerIndex) return false;
        for (const auto& message : messages)
            replies_.push_back(message);
        inFlight_.erase(pending);
        return true;
    }

    // So o game thread pode aplicar Heal/AdvanceWorldTime; a confirmacao
    // e criada APOS ler/escrever o estado real do Terraria.
    bool deliverAction(const NativeCommandRequest& request, NativeCommandAction action) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto pending = inFlight_.find(request.sequence);
        if (request.epoch != epoch_ || pending == inFlight_.end() ||
            actions_.size() >= MaxPending ||
            pending->second.playerIndex != request.playerIndex ||
            pending->second.networkMode != request.networkMode ||
            pending->second.caller != request.caller)
            return false;
        if (action.kind == NativeCommandActionKind::Heal) {
            if (action.amount <= 0 || action.amount > 1000000 ||
                request.caller != NativeCommandRoute::Chat)
                return false;
        } else if (action.kind == NativeCommandActionKind::AdvanceWorldTime) {
            if (action.amount < -86400000 || action.amount > 86400000 ||
                !(request.networkMode == 0 && request.caller == NativeCommandRoute::Chat) &&
                !((request.networkMode & 2) != 0 &&
                  request.caller == NativeCommandRoute::Server))
                return false;
        } else return false;
        action.epoch = request.epoch;
        action.playerIndex = request.playerIndex;
        action.networkMode = request.networkMode;
        action.caller = request.caller;
        actions_.push_back(action);
        inFlight_.erase(pending);
        return true;
    }

    std::vector<NativeCommandAction> takeActions(size_t budget = 8) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<NativeCommandAction> result;
        budget = std::min(budget, MaxPending);
        while (budget-- && !actions_.empty()) {
            result.push_back(actions_.front());
            actions_.pop_front();
        }
        return result;
    }

    // Consumir no thread do jogo; ele decide entre NewText e mensagem de rede.
    std::vector<NativeCommandReply> takeReplies(size_t budget = 16) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<NativeCommandReply> out;
        budget = std::min(budget, MaxReplies);
        out.reserve(std::min(budget, replies_.size()));
        while (budget-- && !replies_.empty()) {
            out.push_back(std::move(replies_.front()));
            replies_.pop_front();
        }
        return out;
    }

    uint64_t epoch() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return epoch_;
    }

    bool hasCommands() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return registeredNames_ != 0;
    }

    size_t pending() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return requests_.size();
    }

    size_t queuedReplies() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return replies_.size();
    }

private:
    struct Ticket {
        int playerIndex = -1;
        int networkMode = 0;
        NativeCommandRoute caller = NativeCommandRoute::Chat;
    };
    struct Registered {
        std::string owner;
        std::string name;
        uint8_t routes = 0;
    };

    static bool validName(const std::string& name) {
        if (name.empty() || name.size() > 64) return false;
        for (unsigned char ch : name)
            if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                  (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.'))
                return false;
        return true;
    }

    static std::string normalize(std::string name) {
        if (!name.empty() && name.front() == '/') name.erase(name.begin());
        if (!validName(name)) return {};
        for (char& ch : name)
            if (ch >= 'A' && ch <= 'Z') ch = char(ch - 'A' + 'a');
        return name;
    }

    static bool matches(uint8_t flags, NativeCommandRoute caller, int netMode) {
        if ((flags & 4) != 0) {
            // 3 e host+cliente: o servidor executa World uma unica vez.
            if ((netMode & 2) != 0) flags |= 2;
            else if (netMode == 0) flags |= 1;
        }
        return (flags & static_cast<uint8_t>(caller)) != 0;
    }

    void invalidateLocked() {
        ++epoch_;
        requests_.clear();
        replies_.clear();
        actions_.clear();
        inFlight_.clear();
    }

    mutable std::mutex mutex_;
    uint64_t epoch_ = 1;
    uint64_t sequence_ = 0;
    size_t registeredNames_ = 0;
    std::map<std::string, std::vector<Registered>> owners_;
    std::map<std::string, std::vector<Registered>> names_;
    std::set<std::string> readyOwners_;
    std::deque<NativeCommandRequest> requests_;
    std::deque<NativeCommandReply> replies_;
    std::deque<NativeCommandAction> actions_;
    std::map<uint64_t, Ticket> inFlight_;
};

// Registro compartilhado entre o loader de fontes, os hooks e o worker Mono.
// Inicialização local evita ordem de construção entre unidades C++.
inline NativeCommandMailbox& nativeCommandMailbox() {
    static NativeCommandMailbox mailbox;
    return mailbox;
}

} // namespace bl::csharp
