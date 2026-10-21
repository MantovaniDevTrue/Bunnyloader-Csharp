#pragma once

#include <cstddef>
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
    int value = 0;
    int count = 1;
    std::shared_ptr<AchievementReply> reply;
};

class ManagedAchievementQueue {
public:
    static constexpr std::size_t Capacity = 256;
    static constexpr std::size_t MaxPayloadBytes = 1024 * 1024;
    static constexpr std::size_t MaxResponseBytes = 1024 * 1024;

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
            job.operation < 0 || job.operation > 8 ||
            job.payload.size() > MaxPayloadBytes) return false;
        if (job.operation >= 1 && job.operation <= 4)
            return job.value >= (job.operation == 1 ? -32768 : 0) &&
                   job.count >= 1 && job.count <= 9999 &&
                   (job.payload == "0" || job.payload == "1");
        if (job.operation == 6) return true;
        return job.payload.empty();
    }

    // Chamador detém g_pinnedMutex. Falha explícita ao atingir a capacidade:
    // eventos não são silenciosamente descartados nem fundidos.
    bool push(AchievementJob job) {
        if (!valid(job) || pending.size() >= Capacity) return false;
        pending.push_back(std::move(job));
        return true;
    }

    bool pop(AchievementJob* job) {
        if (!job || pending.empty()) return false;
        *job = std::move(pending.front());
        pending.pop_front();
        return true;
    }

    std::size_t size() const { return pending.size(); }
    bool empty() const { return pending.empty(); }

private:
    std::deque<AchievementJob> pending;
};

}
