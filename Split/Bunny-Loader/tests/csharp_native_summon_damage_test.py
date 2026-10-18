from pathlib import Path
from tempfile import TemporaryDirectory
import subprocess

root = Path(__file__).resolve().parents[1]
native = (root / "app/src/main/cpp/experiments/csharp/NativeContent.cpp").read_text()
managed = (root / "app/src/main/cpp/experiments/csharp/ManagedProjectileHook.cpp").read_text()
sdk = (root / "experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs").read_text()
compiler = (root / "experiments/csharp/BunnySourceCompiler/CompiledAssemblyProbe.cs").read_text()
loader = (root / "app/src/main/cpp/experiments/csharp/SourceLoader.cpp").read_text()

assert 'parseSignature("void AddBuff(int type, int time, bool fromNetPvP)")' in managed
assert 'parseSignature("void AddBuff(int type, int time, bool quiet)")' not in managed
assert 'nativeManagedMinionConfiguredDamage(type)' in managed
assert 'nativeManagedMinionConfiguredDamage(type)' in native
assert 'hkNativeMinionDamage' in native
assert 'g_nativeMinionProjPet' in native
assert 'g_nativeMinionSacrificable' in native
assert "MinionContactDamage" in sdk
assert 'contactDamage = Value<bool>(projectile, "MinionContactDamage")' in compiler
assert "fields.size() == 17" in loader

program = r"""
#include "experiments/csharp/SummonDamageRules.h"
#include <cassert>
using bl::csharp::resolveNativeSummonDamage;
int main() {
    auto unchanged = resolveNativeSummonDamage(45, 30);
    assert(unchanged.damage == 45 && !unchanged.recovered);
    auto restored = resolveNativeSummonDamage(0, 30);
    assert(restored.damage == 30 && restored.recovered);
    auto negative = resolveNativeSummonDamage(-1, 75);
    assert(negative.damage == 75 && negative.recovered);
    auto noSource = resolveNativeSummonDamage(0, 0);
    assert(noSource.damage == 0 && !noSource.recovered);
    auto oversized = resolveNativeSummonDamage(0, 1001);
    assert(oversized.damage == 0 && !oversized.recovered);
    auto max = resolveNativeSummonDamage(0, 1000);
    assert(max.damage == 1000 && max.recovered);
}
"""
with TemporaryDirectory() as directory:
    code = Path(directory) / "test.cpp"
    binary = Path(directory) / "native-summon-test"
    code.write_text(program)
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                    "-I", str(root / "app/src/main/cpp"),
                    str(code), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

print("BUNNY_CSHARP_SUMMON_DAMAGE_NATIVE=PASS")

example = root / "samples/ExampleModCSharp/Playable/content/Content/Projectiles/ExampleMinion.cs"
staff = root / "samples/ExampleModCSharp/Playable/content/Content/Items/Weapons/Summon/ExampleMinionItem.cs"
assert "MinionContactDamage = true;" in example.read_text()
assert "Summon = true;" in staff.read_text()
policy = (root / "app/src/main/cpp/experiments/csharp/ProjectilePolicy.h").read_text()
assert "fields.size()!=16&&fields.size()!=17" in policy
assert "nativeManagedMinionConfiguredDamage(type)" in native and "nativeManagedMinionConfiguredDamage(type)" in managed
print("BUNNY_OWNED_EXAMPLEMOD_CSHARP_SUMMON=PASS")
