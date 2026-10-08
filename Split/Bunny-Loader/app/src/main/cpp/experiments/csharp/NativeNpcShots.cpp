#include "experiments/csharp/NativeContent.h"
#include "experiments/csharp/CSharpHostTypes.h"
#include "core/Log.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <cmath>
#include <cstdint>

namespace bl::csharp {
namespace {

struct ShotRefs {
    bool tried = false;
    bool ready = false;
    Il2CppClass* parentSource = nullptr;
    const MethodInfo* ctor = nullptr;
    const MethodInfo* newProjectile = nullptr;
};

ShotRefs& refs() {
    static ShotRefs r;
    if (r.tried) return r;
    r.tried = true;
    using namespace bl::il2cpp;
    r.parentSource = findClass({"Terraria.DataStructures", "EntitySource_Parent", {}});
    Il2CppClass* projectile = findClass({"Terraria", "Projectile", {}});
    if (!r.parentSource || !projectile) return r;
    r.ctor = api().class_get_method_from_name(r.parentSource, ".ctor", 2);
    r.newProjectile = findMethodBySignature(projectile, parseSignature(
        "int NewProjectile(IEntitySource spawnSource, float X, float Y, float SpeedX, float SpeedY, int Type, int Damage, float KnockBack, int Owner, float ai0, float ai1, float ai2, NewProjectileModifier modifer)"));
    r.ready = r.ctor && r.newProjectile;
    if (!r.ready) BL_WARN("csharp native NPC: origem ou funcao de projectile indisponivel");
    return r;
}

bool fireOne(const ShotRefs& r, Il2CppObject* source,
             float x, float y, float vx, float vy, int type, int damage) {
    float knockback = 0.f, ai0 = 0.f, ai1 = 0.f, ai2 = 0.f;
    int owner = 255;
    void* args[] = {source, &x, &y, &vx, &vy, &type, &damage,
                    &knockback, &owner, &ai0, &ai1, &ai2, nullptr};
    Il2CppObject* exception = nullptr;
    Il2CppObject* value = bl::il2cpp::api().runtime_invoke(r.newProjectile, nullptr, args, &exception);
    if (exception || !value) return false;
    const int result = *reinterpret_cast<int*>(reinterpret_cast<char*>(value) + sizeof(Il2CppObject));
    return result >= 0 && result < 1000;
}

}

bool fireNativeNpcAttack(Il2CppObject* npc, int type, const HostNpcAttack& attack) {
    if (!npc || nativeNpcProjectileType(type) < 0 ||
        attack.count < 1 || attack.count > 2 ||
        attack.damage < 1 || attack.damage > 100 ||
        !std::isfinite(attack.x) || !std::isfinite(attack.y)) return false;
    const ShotRefs& r = refs();
    if (!r.ready) return false;
    Il2CppObject* source = bl::il2cpp::api().object_new(r.parentSource);
    if (!source) return false;
    void* args[] = {npc, nullptr};
    Il2CppObject* exception = nullptr;
    bl::il2cpp::api().runtime_invoke(r.ctor, source, args, &exception);
    if (exception) return false;
    const int projectileType = nativeNpcProjectileType(type);
    bool fired = fireOne(r, source, attack.x, attack.y,
                         attack.vx0, attack.vy0, projectileType, attack.damage);
    if (attack.count == 2)
        fired = fireOne(r, source, attack.x, attack.y,
                        attack.vx1, attack.vy1, projectileType, attack.damage) && fired;
    return fired;
}

}
