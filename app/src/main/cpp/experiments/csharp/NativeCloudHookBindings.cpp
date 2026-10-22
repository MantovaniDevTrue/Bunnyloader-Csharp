#include "experiments/csharp/NativeCloudHookBindings.h"

#include "il2cpp/Api.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <cstdint>
#include <string>

namespace bl::csharp {
namespace {

bool fail(std::string* error, const std::string& detail) {
    if (error) *error = detail;
    return false;
}

} // namespace

bool resolveNativeCloudHookBindings(NativeCloudHookBindings* bindings,
                                    std::string* error) {
    if (error) error->clear();
    if (!bindings || !error)
        return false;
    *bindings = {};

    auto& a = il2cpp::api();
    if (!a.method_is_instance || !a.field_static_get_value ||
        !a.class_get_element_class || !a.class_get_rank)
        return fail(error, "Stage=CloudHooks.Resolve Reason=MissingIL2CPPExports");

    Il2CppClass* cloud = il2cpp::findClass({"Terraria", "Cloud", {}});
    Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
    if (!cloud || !main)
        return fail(error, "Stage=CloudHooks.Resolve Reason=TerrariaCloudOrMainMissing");

    // Never select a method solely by name/arity. The exact prototypes are
    // specified by Bunny's CloudLoader.js, not by an unrelated tModLoader API.
    const MethodInfo* add = il2cpp::findMethodBySignature(
        cloud, il2cpp::parseSignature("void addCloud()"));
    const MethodInfo* rare = il2cpp::findMethodBySignature(
        cloud, il2cpp::parseSignature("int RollRareCloud()"));
    if (!add || !rare || a.method_is_instance(add) ||
        a.method_is_instance(rare))
        return fail(error, "Stage=CloudHooks.Resolve Reason=CloudMethodSignatureMismatch");

    NativeCloudHookBindings prepared{};
    prepared.addCloud = add;
    prepared.rollRareCloud = rare;
    if (!il2cpp::nativeFieldContract(main, "cloud", "Terraria.Cloud[]",
            true, sizeof(void*), &prepared.mainCloudArray, nullptr, error) ||
        !il2cpp::nativeFieldContract(main, "tenthAnniversaryWorld",
            "System.Boolean", true, 1,
            &prepared.tenthAnniversaryWorld, nullptr, error) ||
        !il2cpp::nativeFieldContract(cloud, "active", "System.Boolean",
            false, 1, nullptr, &prepared.active, error) ||
        !il2cpp::nativeFieldContract(cloud, "type", "System.Int32",
            false, 4, nullptr, &prepared.type, error) ||
        !il2cpp::nativeFieldContract(cloud, "scale", "System.Single",
            false, 4, nullptr, &prepared.scale, error) ||
        !il2cpp::nativeFieldContract(cloud, "width", "System.Int32",
            false, 4, nullptr, &prepared.width, error) ||
        !il2cpp::nativeFieldContract(cloud, "height", "System.Int32",
            false, 4, nullptr, &prepared.height, error) ||
        !il2cpp::nativeFieldContract(cloud, "spriteDir",
            "Microsoft.Xna.Framework.Graphics.SpriteEffects",
            false, 4, nullptr, &prepared.spriteEffects, error))
        return false;

    Il2CppArray* cloudArray = nullptr;
    a.field_static_get_value(prepared.mainCloudArray, &cloudArray);
    if (!cloudArray || cloudArray->length < 200 || cloudArray->length > 2000 ||
        a.class_get_rank(cloudArray->obj.klass) != 1 ||
        a.class_get_element_class(cloudArray->obj.klass) != cloud)
        return fail(error, "Stage=CloudHooks.Resolve Reason=MainCloudArrayContractInvalid");

    *bindings = prepared;
    return true;
}

} // namespace bl::csharp
