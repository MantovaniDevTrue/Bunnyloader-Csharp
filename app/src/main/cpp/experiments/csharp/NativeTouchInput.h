#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace bl::csharp::touch {

constexpr int kMaxButtons = 128;
enum StateBits : uint32_t {
    Current = 1u << 0,
    Old = 1u << 1,
    JustPressed = 1u << 2,
    JustReleased = 1u << 3,
    RetroCurrent = 1u << 4,
    IsBound = 1u << 5,
};

struct Registration {
    std::string owner;
    std::string name;
    std::string label;
    std::string icon;
    bool active = false;
    bool pendingTap = false;
    uint32_t state = 0;
};

class Registry final {
public:
    // O ID é monotônico por sessão. Remover um mod nunca reaproveita IDs.
    int add(std::string_view owner, std::string_view name,
            std::string_view label, std::string_view icon);
    // Pressões registradas entre dois GameUpdateCount sobrevivem ao próximo frame.
    bool tap(int id);
    // IDs referem-se às ações desta sessão, não a enums do Terraria.
    // Nenhum acesso IL2CPP acontece nesta classe.
    int frame(int64_t tick, bool inGame, bool chat, bool npc, bool sign,
              const int* held, size_t heldCount,
              const int* bound, size_t boundCount);
    uint32_t read(int id) const;
    std::string fullName(int id) const;
    std::string displayName(int id) const;
    std::string iconPath(int id) const;
    std::string ownerId(int id) const;
    int count() const;
    void resetWorld();
    void unloadMod(std::string_view owner);

private:
    static bool validName(std::string_view text);
    static bool validLabel(std::string_view text);
    static bool validIcon(std::string_view text);
    bool active(int id) const;
    mutable std::mutex mutex_;
    std::array<Registration, kMaxButtons> entries_{};
    int nextId_ = 0;
    int64_t lastTick_ = -1;
};

Registry& registry();
// Somente o instalador IL2CPP validado pode liberar a leitura C# no jogo.
void setHostReady(bool ready);

} // namespace bl::csharp::touch

extern "C" {
__attribute__((visibility("default"))) int bunny_csharp_touch_register(
    const char* owner, const char* name, const char* label, const char* icon);
__attribute__((visibility("default"))) int bunny_csharp_touch_tap(int id);
__attribute__((visibility("default"))) int bunny_csharp_touch_frame(
    int64_t tick, int inGame, int chat, int npc, int sign,
    const int* held, int heldCount, const int* bound, int boundCount);
__attribute__((visibility("default"))) uint32_t bunny_csharp_touch_read(int id);
__attribute__((visibility("default"))) int bunny_csharp_touch_count();
__attribute__((visibility("default"))) int bunny_csharp_touch_host_ready();
__attribute__((visibility("default"))) void bunny_csharp_touch_world_reset();
__attribute__((visibility("default"))) void bunny_csharp_touch_mod_unload(const char* owner);
}
