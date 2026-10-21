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

struct TouchIconSource {
    std::string owner;
    std::string path;
};

struct Registration {
    std::string owner;
    std::string name;
    std::string label;
    std::string icon;
    bool active = false;
    bool pendingTap = false;
    bool visible = true;
    bool enabled = true;
    uint32_t state = 0;
    uint32_t pressSequence = 0;
};

class Registry final {
public:
    // O ID é monotônico por sessão. Remover um mod nunca reaproveita IDs.
    int add(std::string_view owner, std::string_view name,
            std::string_view label, std::string_view icon);
    // Pressões registradas entre dois GameUpdateCount sobrevivem ao próximo frame.
    bool tap(int id);
    bool policies(int id, bool visible, bool enabled);
    bool visible(int id) const;
    bool enabled(int id) const;
    // IDs referem-se às ações desta sessão, não a enums do Terraria.
    // Nenhum acesso IL2CPP acontece nesta classe.
    int frame(int64_t tick, bool inGame, bool chat, bool npc, bool sign,
              const int* held, size_t heldCount,
              const int* bound, size_t boundCount);
    uint32_t read(int id) const;
    uint64_t pressToken(int id) const;
    int snapshot(uint32_t* states, uint64_t* tokens, int capacity) const;
    std::string fullName(int id) const;
    int find(std::string_view fullName) const;
    std::string displayName(int id) const;
    std::string iconPath(int id) const;
    std::string ownerId(int id) const;
    int iconSources(std::array<TouchIconSource, kMaxButtons>* output) const;
    int count() const;
    uint64_t registrationVersion() const;
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
    uint32_t worldEpoch_ = 1;
    uint64_t registrationVersion_ = 0;
};

Registry& registry();
// Somente o instalador IL2CPP validado pode liberar a leitura C# no jogo.
void setHostReady(bool ready);
bool hostReady();

} // namespace bl::csharp::touch

extern "C" {
__attribute__((visibility("default"))) int bunny_csharp_touch_register(
    const char* owner, const char* name, const char* label, const char* icon);
__attribute__((visibility("default"))) int bunny_csharp_touch_tap(int id);
__attribute__((visibility("default"))) int bunny_csharp_touch_policies(int id, int visible, int enabled);
__attribute__((visibility("default"))) int bunny_csharp_touch_frame(
    int64_t tick, int inGame, int chat, int npc, int sign,
    const int* held, int heldCount, const int* bound, int boundCount);
__attribute__((visibility("default"))) uint32_t bunny_csharp_touch_read(int id);
__attribute__((visibility("default"))) uint64_t bunny_csharp_touch_press_token(int id);
__attribute__((visibility("default"))) int bunny_csharp_touch_snapshot(
    uint32_t* states, uint64_t* tokens, int capacity);
__attribute__((visibility("default"))) int bunny_csharp_touch_count();
__attribute__((visibility("default"))) int bunny_csharp_touch_host_ready();
__attribute__((visibility("default"))) void bunny_csharp_touch_world_reset();
__attribute__((visibility("default"))) void bunny_csharp_touch_mod_unload(const char* owner);
}
