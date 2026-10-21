#include "content/backgrounds/BackgroundTextures.h"
#include "content/common/ContentAssets.h"
#include "content/common/TypeTables.h"
#include "core/Log.h"
#include "boot/Boot.h"
#include <unistd.h>
#include <cctype>
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <filesystem>
#include <vector>
#include <mutex>
#include <sys/stat.h>

namespace bl::runtime::backgrounds {
namespace {
constexpr int maximumSlots = 16384;
constexpr size_t maximumTextures = 4096;
struct Entry { std::string file; int slot; uint32_t asset = 0; int width = 0, height = 0; bool failed = false; bool published = false; std::string failure{}; };
using Key = std::pair<std::string, std::string>;
std::mutex mutex;
std::map<Key, Entry> entries;
int nextSlot = -1;
bool dirty = false;
struct Tables { FieldInfo* assets = nullptr; FieldInfo* widths = nullptr; FieldInfo* heights = nullptr; } tables;

bool fail(std::string* error, const std::string& stage, const std::string& reason,
          const std::string& expected, const std::string& actual) {
    *error = "[CSharp][ERROR][BackgroundTexture] Stage=" + stage + " Expected=" + expected +
        " Actual=" + actual + " Result=Rejected Reason=" + reason;
    return false;
}
bool gameThread(std::string* error) {
    const int expected = bl::runtime::gameThreadId();
    if (expected <= 0 || expected != static_cast<int>(gettid()))
        return fail(error, "ThreadCheck", "WrongThread", "ObservedUnityGameThread:" + std::to_string(expected), std::to_string(gettid()));
    return true;
}
bool identity(const std::string& value, size_t limit, bool path) {
    if (value.empty() || value.size() > limit || value.front() == '/' || value.find("..") != value.npos) return false;
    for (unsigned char c : value)
        if (c < 32 || c == 127 || c == '\\' || c == ':' || c == '|' || (!path && c == '/')) return false;
    return true;
}
std::string normalize(std::string path) {
    if (path.size() >= 4) {
        std::string suffix = path.substr(path.size() - 4);
        std::transform(suffix.begin(), suffix.end(), suffix.begin(), [](unsigned char c) { return std::tolower(c); });
        if (suffix == ".png") path.resize(path.size() - 4);
    }
    return path;
}
bool arrayContract(FieldInfo* field, bool assets, Il2CppArray** array, std::string* error) {
    using namespace il2cpp;
    auto& a = api();
    if (!field || !(a.field_get_flags(field) & 0x10u))
        return fail(error, "BindTables", "MissingOrInstanceField", "StaticArray", "MissingOrInstance");
    auto* cls = a.class_from_il2cpp_type(a.field_get_type(field));
    auto* element = cls && a.class_get_rank(cls) == 1 ? a.class_get_element_class(cls) : nullptr;
    if (!element) return fail(error, "BindTables", "ArrayRankMismatch", "OneDimensionalArray", "MissingOrOtherRank");
    uint32_t align = 0;
    const char* ns = a.class_get_namespace(element);
    const char* name = a.class_get_name(element);
    const bool valid = ns && name && (assets ? element == content::textureAssetClass() && !a.class_is_valuetype(element) :
        a.class_is_valuetype(element) && a.class_value_size(element, &align) == 4 &&
        !std::strcmp(a.class_get_namespace(element), "System") && !std::strcmp(a.class_get_name(element), "Int32"));
    if (!valid) return fail(error, "BindTables", "ArrayElementMismatch", assets ? "Asset<Texture2D>" : "System.Int32", name ? name : "MissingTypeName");
    a.field_static_get_value(field, array);
    if (!*array || (*array)->length == 0 || (*array)->length > maximumSlots || (*array)->bounds)
        return fail(error, "BindTables", "ArrayUnavailableOrOutOfBounds", "InitializedVectorWithin16384", *array ? std::to_string((*array)->length) : "Null");
    return true;
}
bool bind(std::array<Il2CppArray*, 3>* current, std::string* error) {
    using namespace il2cpp;
    auto& a = api();
    if (!a.field_get_flags || !a.field_get_type || !a.class_from_il2cpp_type || !a.class_get_rank ||
        !a.class_get_element_class || !a.class_is_valuetype || !a.class_value_size || !a.class_get_namespace ||
        !a.class_get_name || !a.field_static_get_value || !a.field_static_set_value || !a.gchandle_new ||
        !a.gchandle_free || !a.gchandle_get_target || !a.gc_wbarrier_set_field)
        return fail(error, "BindTables", "MetadataUnavailable", "TypedArraysAndGcExports", "MissingExports");
    if (!tables.assets) {
        auto* main = findClass({"Terraria", "Main", {}});
        auto* textures = findClass({"Terraria.GameContent", "TextureAssets", {}});
        if (!main || !textures) return fail(error, "BindTables", "NativeClassMissing", "MainAndTextureAssets", "Missing");
        tables = {findField(textures, "Background"), findField(main, "backgroundWidth"), findField(main, "backgroundHeight")};
    }
    return arrayContract(tables.assets, true, &(*current)[0], error) &&
        arrayContract(tables.widths, false, &(*current)[1], error) && arrayContract(tables.heights, false, &(*current)[2], error);
}
struct Roots {
    std::array<uint32_t, 3> handles{};
    ~Roots() { for (auto handle : handles) if (handle) il2cpp::api().gchandle_free(handle); }
};
}

int reserve(const std::string& mod, const std::string& rawPath, const std::string& file, std::string* error) {
    const auto path = normalize(rawPath);
    if (!identity(mod, 128, false) || !identity(path, 240, true)) {
        fail(error, "Reserve", "InvalidIdentity", "ModUidAndRelativePath", mod + "/" + path); return -1;
    }
    std::lock_guard<std::mutex> guard(mutex);
    const Key key{mod, path};
    if (auto it = entries.find(key); it != entries.end()) {
        if (it->second.file == file) return it->second.slot;
        fail(error, "Reserve", "IdentityAssetConflict", it->second.file, file); return -1;
    }
    struct stat st{};
    if (stat(file.c_str(), &st) || !S_ISREG(st.st_mode) || st.st_size < 8 || st.st_size > 32 * 1024 * 1024) {
        fail(error, "Reserve", "AssetMissingOrInvalidSize", "ExistingPngWithin32MiB", file); return -1;
    }
    if (nextSlot < 0) {
        auto& a = il2cpp::api();
        auto* main = il2cpp::findClass({"Terraria", "Main", {}});
        auto* count = main ? il2cpp::findField(main, "maxBackgrounds") : nullptr;
        auto* type = count && a.field_get_type && a.class_from_il2cpp_type ? a.class_from_il2cpp_type(a.field_get_type(count)) : nullptr;
        uint32_t align = 0;
        if (!count || !type || !a.field_get_flags || !(a.field_get_flags(count) & 0x10u) ||
            !a.class_is_valuetype || !a.class_is_valuetype(type) || !a.class_value_size ||
            a.class_value_size(type, &align) != 4 || !a.class_get_name || !a.class_get_namespace ||
            std::strcmp(a.class_get_namespace(type), "System") || std::strcmp(a.class_get_name(type), "Int32") || !a.field_static_get_value) {
            fail(error, "Reserve", "NativeCountTypeMismatch", "Static:System.Int32 Main.maxBackgrounds", "MissingOrIncompatible"); return -1;
        }
        int countValue = 0; a.field_static_get_value(count, &countValue);
        if (countValue <= 0 || countValue >= maximumSlots) {
            fail(error, "Reserve", "InvalidVanillaCount", "1..16383", std::to_string(countValue)); return -1;
        }
        nextSlot = countValue;
    }
    if (entries.size() >= maximumTextures || nextSlot >= maximumSlots) {
        fail(error, "Reserve", "CapacityExceeded", "4096TexturesAndSlotBelow16384", std::to_string(nextSlot)); return -1;
    }
    const int slot = nextSlot++;
    entries.emplace(key, Entry{file, slot});
    dirty = true;
    return slot;
}

bool autoload(const std::string& mod, const std::string& directory, std::string* error) {
    std::error_code ec;
    std::vector<std::pair<std::string, std::string>> paths;
    for (std::filesystem::recursive_directory_iterator it(directory, ec), end; it != end && !ec; it.increment(ec)) {
        if (it.depth() > 16) return fail(error, "Autoload", "DirectoryDepthExceeded", "DepthAtMost16", it->path().string());
        if (it->is_symlink(ec)) { it.disable_recursion_pending(); continue; }
        if (!it->is_regular_file(ec) || it->path().extension() != ".png") continue;
        const auto relative = it->path().lexically_relative(directory).generic_string();
        if (!relative.starts_with("Backgrounds/") && relative.find("/Backgrounds/") == relative.npos) continue;
        paths.emplace_back(relative, it->path().string());
        if (paths.size() > maximumTextures) return fail(error, "Autoload", "TextureCountExceeded", "AtMost4096", std::to_string(paths.size()));
    }
    if (ec) return fail(error, "Autoload", "DirectoryReadFailed", "ReadableContentDirectory", ec.message());
    std::sort(paths.begin(), paths.end());
    for (const auto& [path, file] : paths) if (reserve(mod, path, file, error) < 0) return false;
    return true;
}
bool pending() { std::lock_guard<std::mutex> guard(mutex); return dirty; }
int totalSlots() { std::lock_guard<std::mutex> guard(mutex); return nextSlot; }

int find(const std::string& mod, const std::string& path, bool requireReady) {
    std::lock_guard<std::mutex> guard(mutex);
    const auto it = entries.find({mod, normalize(path)});
    return it == entries.end() || (requireReady && (!it->second.published || it->second.failed)) ? -1 : it->second.slot;
}
int count() { std::lock_guard<std::mutex> guard(mutex); return static_cast<int>(entries.size()); }

bool publish(std::string* error, const std::string& modFilter) {
    std::lock_guard<std::mutex> guard(mutex);
    if (entries.empty()) { dirty = false; return true; }
    if (!gameThread(error)) return false;
    auto& a = il2cpp::api();
    std::array<Il2CppArray*, 3> arrays{};
    if (!bind(&arrays, error)) return false;
    bool complete = true;
    for (auto& [key, entry] : entries) {
        if (entry.failed) { if (modFilter.empty() || key.first == modFilter) { complete = false; if (error->empty()) *error = entry.failure; } continue; }
        if (entry.asset) continue;
        auto* asset = content::loadTextureAsset(entry.file, nullptr, 0, key.first + "/" + key.second, &entry.width, &entry.height);
        if (!asset || entry.width <= 0 || entry.height <= 0 || entry.width > 16384 || entry.height > 16384) {
            if (asset) content::destroyTextureAsset(asset);
            entry.failed = true; if (modFilter.empty() || key.first == modFilter) complete = false;
            fail(error, "TextureLoad", "TextureDecodeOrDimensionsInvalid", "LoadedTextureWithin16384", entry.file);
            entry.failure = *error + " Mod=" + key.first + " Path=" + key.second + " ContentId=" + std::to_string(entry.slot);
            BL_ERROR("%s", entry.failure.c_str());
            continue;
        }
        entry.asset = a.gchandle_new(asset, true);
        if (!entry.asset) {
            content::destroyTextureAsset(asset); entry.failed = true; if (modFilter.empty() || key.first == modFilter) complete = false;
            fail(error, "RetainAsset", "GcRootAllocationFailed", "NonZeroGcHandle", "0");
            entry.failure = *error + " Mod=" + key.first + " Path=" + key.second;
        }
    }
    // Build all three replacement arrays before publishing any field. Re-read
    // roots after allocations; IL2CPP may collect while loading another texture.
    if (!bind(&arrays, error)) return false;
    Roots roots;
    for (size_t i = 0; i < arrays.size(); ++i) {
        auto* staged = TypeTables::resizedCopy(arrays[i], std::max<size_t>(nextSlot, arrays[i]->length));
        if (!staged || !(roots.handles[i] = a.gchandle_new(reinterpret_cast<Il2CppObject*>(staged), true)))
            return fail(error, "StageTables", "AllocationFailed", "ThreeRootedArrays", std::to_string(i));
    }
    for (const auto& [key, entry] : entries) {
        (void)key;
        if (!entry.asset || entry.failed) { if (modFilter.empty() || key.first == modFilter) complete = false; continue; }
        auto* textures = reinterpret_cast<Il2CppArray*>(a.gchandle_get_target(roots.handles[0]));
        auto* widths = reinterpret_cast<Il2CppArray*>(a.gchandle_get_target(roots.handles[1]));
        auto* heights = reinterpret_cast<Il2CppArray*>(a.gchandle_get_target(roots.handles[2]));
        auto** slot = static_cast<void**>(arrayData(textures)) + entry.slot;
        a.gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(textures), slot, a.gchandle_get_target(entry.asset));
        static_cast<int32_t*>(arrayData(widths))[entry.slot] = entry.width;
        static_cast<int32_t*>(arrayData(heights))[entry.slot] = entry.height;
    }
    const std::array<FieldInfo*, 3> fields{tables.assets, tables.widths, tables.heights};
    for (size_t i = 0; i < fields.size(); ++i) {
        auto* value = a.gchandle_get_target(roots.handles[i]);
        // IL2CPP StaticSetValue takes a reference object directly. Value-type
        // fields take a pointer to their bytes (see bridge/Bindings.cpp).
        a.field_static_set_value(fields[i], value);
    }
    dirty = false;
    for (auto& [key, entry] : entries) { (void)key; entry.published = entry.asset && !entry.failed; }
    if (complete) error->clear();
    return complete;
}

int readiness(const std::string& mod, std::string* error) {
    std::lock_guard<std::mutex> guard(mutex);
    int state = 1;
    for (const auto& [key, entry] : entries) {
        if (key.first != mod) continue;
        if (entry.failed) { if (error) *error = entry.failure; return -1; }
        if (!entry.published) state = 0;
    }
    return state;
}

void unload(const std::string& mod) {
    std::string error;
    std::lock_guard<std::mutex> guard(mutex);
    if (std::none_of(entries.begin(), entries.end(), [&](const auto& pair) { return pair.first.first == mod; })) return;
    if (!gameThread(&error)) { BL_ERROR("%s Mod=%s", error.c_str(), mod.c_str()); return; }
    auto& a = il2cpp::api();
    std::array<Il2CppArray*, 3> arrays{};
    const bool needsTables = std::any_of(entries.begin(), entries.end(), [&](const auto& pair) { return pair.first.first == mod && pair.second.published; });
    if (needsTables && !bind(&arrays, &error)) { BL_ERROR("%s Mod=%s", error.c_str(), mod.c_str()); return; }
    for (auto it = entries.begin(); it != entries.end();) {
        if (it->first.first != mod) { ++it; continue; }
        const auto& entry = it->second;
        auto* asset = entry.asset ? a.gchandle_get_target(entry.asset) : nullptr;
        if (asset && entry.published && arrays[0] && entry.slot < static_cast<int>(arrays[0]->length)) {
            auto** slot = static_cast<void**>(arrayData(arrays[0])) + entry.slot;
            if (*slot == asset) {
                a.gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(arrays[0]), slot, nullptr);
                if (entry.slot < static_cast<int>(arrays[1]->length)) static_cast<int32_t*>(arrayData(arrays[1]))[entry.slot] = 0;
                if (entry.slot < static_cast<int>(arrays[2]->length)) static_cast<int32_t*>(arrayData(arrays[2]))[entry.slot] = 0;
            }
        }
        if (asset) content::destroyTextureAsset(asset);
        if (entry.asset) a.gchandle_free(entry.asset);
        it = entries.erase(it);
    }
}
}

extern "C" __attribute__((visibility("default")))
int bunny_background_texture_slot(const char* mod, const char* path) {
    if (!mod || !path || strnlen(mod, 129) > 128 || strnlen(path, 241) > 240) return -1;
    return bl::runtime::backgrounds::find(mod, path);
}
