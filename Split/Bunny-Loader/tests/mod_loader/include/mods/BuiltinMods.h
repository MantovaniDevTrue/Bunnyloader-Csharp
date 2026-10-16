#pragma once
namespace bl::mods {
struct BuiltinMod {const char* id;const char* source;};
inline const BuiltinMod kBuiltinMods[]={{"fixture","export default class Fixture {}"}};
inline constexpr int kBuiltinModCount=1;
}
