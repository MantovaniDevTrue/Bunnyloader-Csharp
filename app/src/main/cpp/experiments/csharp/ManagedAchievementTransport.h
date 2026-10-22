#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <utility>

namespace bl::csharp {

// Buffer FIFO usado exclusivamente sob g_pinnedMutex do worker MonoVM.
// Eventos retornam imediatamente ao jogo; controles possuem resposta explícita.
struct AchievementReply {
    bool started = false;
    bool done = false;
    bool cancelled = false;
    std::string output;
};

struct AchievementJob {
    std::string mod;
    std::string assembly;
    std::string payload;
    int operation = 0;
    std::uint64_t generation = 0;
    int value = 0;
    int count = 1;
    std::shared_ptr<AchievementReply> reply;
};

class ManagedAchievementQueue {
public:
    static constexpr std::size_t Capacity = 256;
    static constexpr std::size_t MaxPayloadBytes = 1024 * 1024;
    static constexpr std::size_t MaxResponseBytes = 1024 * 1024;
    static constexpr std::size_t MaxBufferedPayloadBytes = 2 * 1024 * 1024;

    static bool validModId(const std::string& name) {
        if (name.empty() || name.size() > 96) return false;
        for (const unsigned char c : name) {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
                return false;
        }
        return true;
    }

    static bool valid(const AchievementJob& job) {
        if (!validModId(job.mod) || job.assembly.empty() || job.assembly.size() > 1024 ||
            job.operation < 0 || job.operation > 11 ||
            job.payload.size() > MaxPayloadBytes) return false;
        if (job.operation >= 1 && job.operation <= 4)
            return job.value >= (job.operation == 1 ? -32768 : 0) &&
                   job.count >= 1 && job.count <= 9999 &&
                   (job.payload == "0" || job.payload == "1");
        if (job.operation == 6) return true;
        if (job.operation == 9 || job.operation == 11)
            return !job.payload.empty() && job.payload.size() <= 2048 && job.payload.front() == '/';
        return job.payload.empty();
    }

    // Chamador detém g_pinnedMutex. Falha explícita ao atingir a capacidade:
    // eventos não são silenciosamente descartados nem fundidos.
    bool push(AchievementJob job) {
        if (!valid(job) || pending.size() >= Capacity ||
            job.payload.size() > MaxBufferedPayloadBytes - bufferedBytes)
            return false;
        bufferedBytes += job.payload.size();
        pending.push_back(std::move(job));
        return true;
    }

    bool pop(AchievementJob* job) {
        if (!job || pending.empty()) return false;
        *job = std::move(pending.front());
        bufferedBytes -= job->payload.size();
        pending.pop_front();
        return true;
    }

    // Called under the existing MonoVM queue mutex when a world/profile ends.
    // Control RPCs, saves and unloads remain in their original FIFO order.
    std::size_t discardPendingEvents() {
        std::size_t removed = 0;
        for (auto it = pending.begin(); it != pending.end();) {
            if (it->operation >= 1 && it->operation <= 4) {
                bufferedBytes -= it->payload.size();
                it = pending.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }

    std::size_t size() const { return pending.size(); }
    bool empty() const { return pending.empty(); }

private:
    std::deque<AchievementJob> pending;
    std::size_t bufferedBytes = 0;
};

}
