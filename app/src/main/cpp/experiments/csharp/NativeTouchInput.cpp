#include "experiments/csharp/NativeTouchInput.h"
#include <algorithm>
#include <array>
#include <atomic>

namespace bl::csharp::touch {

namespace { std::atomic<bool> g_hostReady{false}; }

bool Registry::validName(std::string_view value) {
    if (value.empty() || value.size() > 80 || value == "." || value == "..")
        return false;
    for (unsigned char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
            return false;
    return true;
}

bool Registry::validLabel(std::string_view label) {
    if (label.empty() || label.size() > 100) return false;
    for (unsigned char c : label)
        if (c < 32 || c == 127) return false;
    return true;
}

bool Registry::validIcon(std::string_view path) {
    if (path.empty() || path.size() > 200 || path[0] == '/' ||
        path.find('\\') != path.npos || path.find(':') != path.npos ||
        path.find('|') != path.npos)
        return false;
    size_t start = 0;
    while (start < path.size()) {
        size_t end = path.find('/', start);
        if (end == path.npos) end = path.size();
        auto part = path.substr(start, end - start);
        if (part.empty() || part == "." || part == "..") return false;
        for (unsigned char c : part)
            if (c < 32 || c == 127) return false;
        start = end + 1;
    }
    return path.back() != '/';
}

bool Registry::active(int id) const {
    return id >= 0 && id < nextId_ &&
           entries_[static_cast<size_t>(id)].active;
}

int Registry::add(std::string_view owner, std::string_view name,
                  std::string_view label, std::string_view icon) {
    if (!validName(owner) || !validName(name) ||
        !validLabel(label) || !validIcon(icon)) return -1;
    std::lock_guard lock(mutex_);
    for (int i = 0; i < nextId_; ++i) {
        const Registration& entry = entries_[static_cast<size_t>(i)];
        if (entry.active && entry.owner == owner && entry.name == name) return i;
    }
    if (nextId_ >= kMaxButtons) return -2;
    int id = nextId_++;
    Registration& entry = entries_[static_cast<size_t>(id)];
    entry.owner = owner;
    entry.name = name;
    entry.label = label;
    entry.icon = icon;
    entry.active = true;
    entry.pendingTap = false;
    entry.visible = true;
    entry.enabled = true;
    entry.state = 0;
    entry.pressSequence = 0;
    ++registrationVersion_;
    return id;
}

bool Registry::tap(int id) {
    std::lock_guard lock(mutex_);
    if (!active(id) || !entries_[static_cast<size_t>(id)].visible ||
        !entries_[static_cast<size_t>(id)].enabled) return false;
    entries_[static_cast<size_t>(id)].pendingTap = true;
    return true;
}
bool Registry::policies(int id, bool visible, bool enabled) {
    std::lock_guard lock(mutex_);
    if (!active(id)) return false;
    auto& item = entries_[static_cast<size_t>(id)];
    item.visible = visible;
    item.enabled = enabled;
    if (!visible || !enabled) item.pendingTap = false;
    return true;
}
bool Registry::visible(int id) const {
    std::lock_guard lock(mutex_);
    return active(id) && entries_[static_cast<size_t>(id)].visible;
}
bool Registry::enabled(int id) const {
    std::lock_guard lock(mutex_);
    return active(id) && entries_[static_cast<size_t>(id)].enabled;
}

int Registry::frame(int64_t tick, bool inGame, bool chat, bool npc, bool sign,
                    const int* held, size_t heldCount,
                    const int* bound, size_t boundCount) {
    if (tick < 0 || heldCount > kMaxButtons || boundCount > kMaxButtons ||
        (heldCount && !held) || (boundCount && !bound)) return -1;
    std::lock_guard lock(mutex_);
    if (tick <= lastTick_) return 0;
    std::array<bool, kMaxButtons> heldMask{};
    std::array<bool, kMaxButtons> boundMask{};
    for (size_t i = 0; i < heldCount; ++i)
        if (active(held[i])) heldMask[static_cast<size_t>(held[i])] = true;
    for (size_t i = 0; i < boundCount; ++i)
        if (active(bound[i])) boundMask[static_cast<size_t>(bound[i])] = true;
    const bool retro = inGame && !chat && !npc && !sign;
    int changes = 0;
    for (int i = 0; i < nextId_; ++i) {
        Registration& entry = entries_[static_cast<size_t>(i)];
        if (!entry.active) continue;
        bool previous = (entry.state & Current) != 0;
        bool down = inGame && entry.visible && entry.enabled &&
            (heldMask[static_cast<size_t>(i)] || entry.pendingTap);
        uint32_t state = (down ? Current : 0u) | (previous ? Old : 0u);
        if (down && !previous) {
            state |= JustPressed;
            if (++entry.pressSequence == 0) entry.pressSequence = 1;
        }
        if (previous && !down) state |= JustReleased;
        if (down && retro) state |= RetroCurrent;
        if (boundMask[static_cast<size_t>(i)]) state |= IsBound;
        entry.state = state;
        entry.pendingTap = false;
        if (state & (JustPressed | JustReleased)) ++changes;
    }
    lastTick_ = tick;
    return changes;
}

uint64_t Registry::pressToken(int id) const {
    std::lock_guard lock(mutex_);
    if (!active(id)) return 0;
    return (static_cast<uint64_t>(worldEpoch_) << 32) |
           entries_[static_cast<size_t>(id)].pressSequence;
}

int Registry::snapshot(uint32_t* states, uint64_t* tokens, int capacity) const {
    if (!states || !tokens || capacity < 1 || capacity > kMaxButtons)
        return -1;
    std::lock_guard lock(mutex_);
    if (capacity < nextId_) return -1;
    for (int i = 0; i < capacity; ++i) {
        if (!active(i)) {
            states[i] = 0;
            tokens[i] = 0;
            continue;
        }
        const Registration& entry = entries_[static_cast<size_t>(i)];
        states[i] = entry.state;
        tokens[i] = (static_cast<uint64_t>(worldEpoch_) << 32) |
                    entry.pressSequence;
    }
    return nextId_;
}

uint32_t Registry::read(int id) const {
    std::lock_guard lock(mutex_);
    return active(id) ? entries_[static_cast<size_t>(id)].state : 0u;
}

std::string Registry::fullName(int id) const {
    std::lock_guard lock(mutex_);
    if (!active(id)) return {};
    const auto& e = entries_[static_cast<size_t>(id)];
    return e.owner + "/" + e.name;
}
int Registry::find(std::string_view fullName) const {
    std::lock_guard lock(mutex_);
    for (int i = 0; i < nextId_; ++i) {
        const auto& e = entries_[static_cast<size_t>(i)];
        if (!e.active) continue;
        const size_t slash = fullName.find('/');
        if (slash == std::string_view::npos) return -1;
        if (fullName.substr(0, slash) == e.owner &&
            fullName.substr(slash + 1) == e.name) return i;
    }
    return -1;
}
std::string Registry::displayName(int id) const {
    std::lock_guard lock(mutex_);
    return active(id) ? entries_[static_cast<size_t>(id)].label : std::string{};
}
std::string Registry::iconPath(int id) const {
    std::lock_guard lock(mutex_);
    return active(id) ? entries_[static_cast<size_t>(id)].icon : std::string{};
}
std::string Registry::ownerId(int id) const {
    std::lock_guard lock(mutex_);
    return active(id) ? entries_[static_cast<size_t>(id)].owner : std::string{};
}

int Registry::count() const {
    std::lock_guard lock(mutex_);
    int result = 0;
    for (int i = 0; i < nextId_; ++i)
        if (entries_[static_cast<size_t>(i)].active) ++result;
    return result;
}

uint64_t Registry::registrationVersion() const {
    std::lock_guard lock(mutex_);
    return registrationVersion_;
}

void Registry::resetWorld() {
    std::lock_guard lock(mutex_);
    lastTick_ = -1;
    if (++worldEpoch_ == 0) worldEpoch_ = 1;
    for (int i = 0; i < nextId_; ++i) {
        Registration& e = entries_[static_cast<size_t>(i)];
        e.pendingTap = false;
        e.state = 0;
        e.pressSequence = 0;
    }
}

void Registry::unloadMod(std::string_view owner) {
    std::lock_guard lock(mutex_);
    bool changed = false;
    for (int i = 0; i < nextId_; ++i) {
        Registration& e = entries_[static_cast<size_t>(i)];
        if (e.owner != owner || !e.active) continue;
        e.active = false;
        e.pendingTap = false;
        e.state = 0;
        changed = true;
    }
    if (changed) ++registrationVersion_;
}

Registry& registry() {
    static Registry value;
    return value;
}

void setHostReady(bool ready) {
    const bool previous = g_hostReady.exchange(ready, std::memory_order_acq_rel);
    if (previous && !ready) registry().resetWorld();
}

bool hostReady() { return g_hostReady.load(std::memory_order_acquire); }

} // namespace bl::csharp::touch

extern "C" __attribute__((visibility("default"))) int bunny_csharp_touch_register(
    const char* owner, const char* name, const char* label, const char* icon) {
    if (!owner || !name || !label || !icon) return -1;
    return bl::csharp::touch::registry().add(owner, name, label, icon);
}
extern "C" __attribute__((visibility("default"))) int bunny_csharp_touch_tap(int id) {
    return bl::csharp::touch::registry().tap(id) ? 1 : 0;
}
extern "C" __attribute__((visibility("default"))) int bunny_csharp_touch_policies(
    int id, int visible, int enabled) {
    return bl::csharp::touch::registry().policies(id, visible != 0, enabled != 0) ? 1 : 0;
}
extern "C" __attribute__((visibility("default"))) int bunny_csharp_touch_frame(
    int64_t tick, int inGame, int chat, int npc, int sign,
    const int* held, int heldCount, const int* bound, int boundCount) {
    if (heldCount < 0 || boundCount < 0) return -1;
    return bl::csharp::touch::registry().frame(tick, inGame != 0, chat != 0,
        npc != 0, sign != 0, held, static_cast<size_t>(heldCount),
        bound, static_cast<size_t>(boundCount));
}
extern "C" __attribute__((visibility("default"))) uint32_t bunny_csharp_touch_read(int id) {
    return bl::csharp::touch::registry().read(id);
}
extern "C" __attribute__((visibility("default"))) uint64_t bunny_csharp_touch_press_token(int id) {
    return bl::csharp::touch::registry().pressToken(id);
}
extern "C" __attribute__((visibility("default"))) int bunny_csharp_touch_snapshot(
    uint32_t* states, uint64_t* tokens, int capacity) {
    return bl::csharp::touch::registry().snapshot(states, tokens, capacity);
}
extern "C" __attribute__((visibility("default"))) int bunny_csharp_touch_host_ready() {
    return bl::csharp::touch::hostReady() ? 1 : 0;
}
extern "C" __attribute__((visibility("default"))) int bunny_csharp_touch_count() {
    return bl::csharp::touch::registry().count();
}
extern "C" __attribute__((visibility("default"))) void bunny_csharp_touch_world_reset() {
    bl::csharp::touch::registry().resetWorld();
}
extern "C" __attribute__((visibility("default"))) void bunny_csharp_touch_mod_unload(
    const char* owner) {
    if (owner) bl::csharp::touch::registry().unloadMod(owner);
}
