#pragma once

#include <string>
#include <string_view>

namespace bl::csharp::hair {

// Única entrada do arquivo escrito pelo worker MonoVM. O diretório deve
// ser privado do app, e o nome do arquivo é derivado de UID validado;
// nada de passar caminhos arbitrários vindos de um mod.
std::string NativeHairManifestName(std::string_view uid);

bool ReadNativeHairManifest(std::string_view workDir, std::string_view uid,
                            std::string* manifest, std::string* error);

} // namespace bl::csharp::hair
