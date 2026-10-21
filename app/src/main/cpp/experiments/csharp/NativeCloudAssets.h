#pragma once

#include "experiments/csharp/NativeCloudRegistry.h"

#include <map>
#include <string>

namespace bl::csharp {

// Only call on Terraria's game thread, after the vanilla mask manager's
// LoadMasks/Awake stage. No JS bridge, no MonoVM calls in the renderer.
// IMPORTANT: source mods remain disabled by SourceLoader until this API and
// the IL2CPP cloud-selection hooks are wired into the ready lifecycle.
bool installNativeCloudAssets(NativeCloudRegistry* registry,
                              const std::map<std::string, std::string>& modDirectories,
                              std::string* error);

// The caller must first stop mod cloud selection and remove/expire all active
// mod cloud instances. Never destroy a cloud Texture2D during rendering.
bool uninstallNativeCloudAssets(std::string* error);
bool nativeCloudAssetsInstalled();

} // namespace bl::csharp
