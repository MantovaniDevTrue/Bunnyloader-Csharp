#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace bl::csharp {

enum class ManagedVisualKind : uint8_t {
    Particle = 1,
    PlayerOverlay = 2,
    ClearOverlay = 3,
    EquipmentSprite = 4
};

struct ManagedVisualCommand {
    ManagedVisualKind kind = ManagedVisualKind::Particle;
    int playerIndex = -1;
    int effectId = -1;
    std::string texture;
    float x = 0;
    float y = 0;
    float scale = 1;
    float opacity = 1;
    uint32_t color = 0xffffffffu;
    int durationTicks = 0;
    int style = 0;
};

class ManagedVisualQueue {
public:
    static constexpr size_t Capacity = 64;

    bool push(ManagedVisualCommand command) {
        if (command.playerIndex < 0 || command.effectId < 0 ||
            command.texture.size() > 240 || command.durationTicks < 0 ||
            !(command.scale > 0 && command.scale < 10000) ||
            !(command.opacity >= 0 && command.opacity <= 1)) return false;
        std::lock_guard<std::mutex> guard(lock_);
        if (command.kind != ManagedVisualKind::Particle) {
            for (size_t i = 0; i < size_; ++i) {
                auto& old = buffer_[(head_ + i) % Capacity];
                if (old.playerIndex != command.playerIndex ||
                    old.effectId != command.effectId) continue;
                const bool overlays =
                    (old.kind == ManagedVisualKind::PlayerOverlay ||
                     old.kind == ManagedVisualKind::ClearOverlay) &&
                    (command.kind == ManagedVisualKind::PlayerOverlay ||
                     command.kind == ManagedVisualKind::ClearOverlay);
                if (old.kind == command.kind || overlays) {
                    old = std::move(command);
                    return true;
                }
            }
        }
        if (size_ == Capacity) return false;
        buffer_[(head_ + size_) % Capacity] = std::move(command);
        ++size_;
        return true;
    }

    bool pop(ManagedVisualCommand& out) {
        std::lock_guard<std::mutex> guard(lock_);
        if (!size_) return false;
        out = std::move(buffer_[head_]);
        head_ = (head_ + 1) % Capacity;
        --size_;
        return true;
    }

    void clear() {
        std::lock_guard<std::mutex> guard(lock_);
        head_ = 0;
        size_ = 0;
        for (auto& command : buffer_) command = {};
    }

    size_t size() const {
        std::lock_guard<std::mutex> guard(lock_);
        return size_;
    }

private:
    mutable std::mutex lock_;
    std::array<ManagedVisualCommand, Capacity> buffer_{};
    size_t head_ = 0;
    size_t size_ = 0;
};

}
