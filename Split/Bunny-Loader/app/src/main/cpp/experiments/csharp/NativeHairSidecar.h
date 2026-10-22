#pragma once

#include <string>
#include <string_view>

namespace bl::csharp::hair {

// Sidecar separado do PlayerLoader JS: não sobrescreve .plr.bl.json ou
// qualquer SaveData de outros mods. Apenas jogadores locais (não cloud).
// Um mod temporariamente desativado mantém sua chave para restauração futura.
enum class SidecarRead { Found, Missing, Invalid, IOError };

std::string NativeHairSidecarPath(std::string_view terrariaPlayerPath);

bool RemoveNativeHairSidecar(std::string_view terrariaPlayerPath, std::string* error);

bool WriteNativeHairSidecar(std::string_view terrariaPlayerPath,
                            std::string_view stableKey, std::string* error);

SidecarRead ReadNativeHairSidecar(std::string_view terrariaPlayerPath,
                                  std::string* stableKey, std::string* error);

} // namespace bl::csharp::hair
