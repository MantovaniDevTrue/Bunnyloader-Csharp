#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace bl::csharp::banners {

struct Binding {
    std::string modId;
    std::string npcClass;
    std::string representativeClass;
    std::string itemClass;
    int npcType = -1;
    int representativeType = -1;
    int itemType = -1;
};

struct Snapshot {
    std::map<std::string, std::vector<Binding>> byMod;
    std::map<int, int> npcToBanner;
    std::map<int, int> bannerToItem;
    std::map<int, int> itemToBanner;
};

// Publica tabelas completas por snapshot imutavel. Nenhuma consulta do
// BannerSystem precisa travar a thread de render ou chamar o MonoVM.
// O hook nativo IL2CPP ainda nao esta instalado: esta classe nao finge isso.
class Registry final {
public:
    explicit Registry(int firstModNpc) : firstModNpc_(firstModNpc) {}

    bool validate(const std::string& modId, const std::vector<Binding>& definitions,
                  std::string* error) const {
        if (!error) return false;
        auto current = std::atomic_load_explicit(&state_, std::memory_order_acquire);
        Snapshot next = *current;
        if (!modId.empty()) next.byMod[modId] = definitions;
        return rebuild(next, error);
    }

    bool publish(const std::string& modId, const std::vector<Binding>& definitions,
                 std::string* error) {
        if (!error) return false;
        std::lock_guard<std::mutex> lock(writer_);
        auto current = std::atomic_load_explicit(&state_, std::memory_order_acquire);
        Snapshot next = *current;
        next.byMod[modId] = definitions;
        if (!rebuild(next, error)) return false;
        std::shared_ptr<const Snapshot> committed = std::make_shared<Snapshot>(std::move(next));
        std::atomic_store_explicit(&state_, std::move(committed), std::memory_order_release);
        return true;
    }

    void unload(const std::string& modId) {
        std::lock_guard<std::mutex> lock(writer_);
        auto current = std::atomic_load_explicit(&state_, std::memory_order_acquire);
        Snapshot next = *current;
        next.byMod.erase(modId);
        std::string error;
        if (!rebuild(next, &error)) return;
        std::shared_ptr<const Snapshot> committed = std::make_shared<Snapshot>(std::move(next));
        std::atomic_store_explicit(&state_, std::move(committed), std::memory_order_release);
    }

    void clear() {
        std::lock_guard<std::mutex> lock(writer_);
        std::shared_ptr<const Snapshot> empty = std::make_shared<Snapshot>();
        std::atomic_store_explicit(&state_, std::move(empty), std::memory_order_release);
    }

    int npcToBanner(int npcType) const {
        auto state = snapshot();
        auto it = state->npcToBanner.find(npcType);
        return it == state->npcToBanner.end() ? -1 : it->second;
    }

    int bannerToItem(int bannerType) const {
        auto state = snapshot();
        auto it = state->bannerToItem.find(bannerType);
        return it == state->bannerToItem.end() ? -1 : it->second;
    }

    int bannerToNpc(int bannerType) const {
        auto state = snapshot();
        return state->bannerToItem.contains(bannerType) ? bannerType : -1;
    }

    std::shared_ptr<const Snapshot> snapshot() const {
        return std::atomic_load_explicit(&state_, std::memory_order_acquire);
    }

private:
    int firstModNpc_;
    mutable std::mutex writer_;
    std::shared_ptr<const Snapshot> state_ = std::make_shared<Snapshot>();

    static bool validPart(const std::string& str) {
        if (str.empty() || str.size() > 128) return false;
        for (unsigned char ch : str)
            if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                  (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.'))
                return false;
        return str != "." && str != "..";
    }

    bool rebuild(Snapshot& next, std::string* error) const {
        if (firstModNpc_ < 1 || firstModNpc_ >= 262144) {
            *error = "BannerLoader: limite vanilla invalido";
            return false;
        }
        next.npcToBanner.clear();
        next.bannerToItem.clear();
        next.itemToBanner.clear();
        for (const auto& [modId, all] : next.byMod) {
            if (!validPart(modId) || all.size() > 4096) {
                *error = "BannerLoader: dono ou quantidade invalidos";
                return false;
            }
            std::map<std::string, const Binding*> representatives;
            std::set<std::string> classes;
            for (const auto& binding : all) {
                if (binding.modId != modId ||
                    !validPart(binding.npcClass) ||
                    !validPart(binding.representativeClass) ||
                    !validPart(binding.itemClass) ||
                    binding.npcType < firstModNpc_ || binding.npcType >= 262144 ||
                    binding.representativeType < firstModNpc_ ||
                    binding.representativeType >= 262144 ||
                    binding.itemType <= 0 ||
                    !classes.insert(binding.npcClass).second) {
                    *error = "BannerLoader: classe, owner ou tipo invalido";
                    return false;
                }
                representatives.emplace(binding.npcClass, &binding);
            }
            for (const auto& binding : all) {
                auto rep = representatives.find(binding.representativeClass);
                if (rep == representatives.end() ||
                    rep->second->npcType != binding.representativeType ||
                    rep->second->representativeType != binding.representativeType ||
                    rep->second->itemType != binding.itemType ||
                    rep->second->representativeClass != binding.representativeClass ||
                    !next.npcToBanner.emplace(binding.npcType, binding.representativeType).second) {
                    *error = "BannerLoader: representante ausente, ciclo ou colisao de NPC";
                    return false;
                }
                auto [item, isNew] = next.bannerToItem.emplace(
                    binding.representativeType, binding.itemType);
                if (!isNew && item->second != binding.itemType) {
                    *error = "BannerLoader: bandeira vinculada a itens diferentes";
                    return false;
                }
                auto [banner, created] = next.itemToBanner.emplace(
                    binding.itemType, binding.representativeType);
                if (!created && banner->second != binding.representativeType) {
                    *error = "BannerLoader: mesmo item usado por bandeiras diferentes";
                    return false;
                }
            }
        }
        error->clear();
        return true;
    }
};

inline Registry& registry(int firstModNpc) {
    // Primeiro acesso feito pelo SourceLoader com kVanillaNpcCount real.
    static Registry active(firstModNpc);
    return active;
}

}  // namespace bl::csharp::banners
