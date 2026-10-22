#include "experiments/csharp/NativeHairWorkerPipeline.h"

#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/NativeHairHandoff.h"

namespace bl::csharp::hair {

WorkerHairResult PrepareOneCompiledHairMod(
    NativeHairStartup* startup, std::string_view uid,
    int verifiedHairClassCount, std::string_view privateWorkDir,
    const ManagedHairExporter& exportManifest,
    const Registry::AssetExists& assetExists) {
    WorkerHairResult out;
    out.count = verifiedHairClassCount;
    if (!startup || !validIdentifier(uid) ||
        verifiedHairClassCount < 1 || verifiedHairClassCount > 2048 ||
        !exportManifest || !assetExists) {
        out.phase = "hair-worker-invalid-arguments";
        out.error = "HairWorkerInvalidArguments";
        return out;
    }

    NativeHairStartup::Block reserved;
    if (!startup->Begin(uid, verifiedHairClassCount, &reserved, &out.error)) {
        out.phase = "hair-worker-slot-reservation-failed";
        return out;
    }
    out.firstType = reserved.first;

    const auto fail = [&](const char* phase) {
        out.phase = phase;
        startup->Reset();
        return out;
    };

    int exported = -1;
    try {
        exported = exportManifest(reserved.first);
    } catch (...) {
        out.error = "HairWorkerManagedExportException";
        return fail("hair-worker-managed-exception");
    }
    if (exported != verifiedHairClassCount) {
        out.error = "HairWorkerManagedExportCountMismatch";
        return fail("hair-worker-managed-count-mismatch");
    }

    std::string wire;
    if (!ReadNativeHairManifest(privateWorkDir, uid, &wire, &out.error))
        return fail("hair-worker-file-handoff-failed");

    if (!startup->CommitManifest(uid, wire, assetExists, &out.error))
        return fail("hair-worker-native-manifest-invalid");

    if (!startup->Seal(&out.error))
        return fail("hair-worker-slots-changed-before-seal");

    const Registry* registry = startup->ReadyDescriptors();
    if (!registry || registry->Size() != static_cast<size_t>(verifiedHairClassCount)) {
        out.error = "HairWorkerSealedRegistryMismatch";
        return fail("hair-worker-final-verify-failed");
    }
    out.ready = true;
    out.phase = "hair-worker-descriptors-ready";
    return out;
}

WorkerHairResult PrepareCompiledHairModOnWorker(
    NativeHairStartup* startup,
    const std::string& probeAssembly, const std::string& compilerAssembly,
    const std::string& compiledModAssembly, const std::string& modUid,
    const std::string& modRoot, const std::string& privateWorkDir,
    const Registry::AssetExists& assetExists) {
    const auto detected = bl::csharp::inspectManagedHairClassCount(
        probeAssembly, compilerAssembly, compiledModAssembly);
    if (detected.phase != "hair-count-verified" || detected.classes < 1) {
        WorkerHairResult result;
        result.phase = "hair-worker-class-inspection-failed";
        result.error = detected.error.empty()
            ? "HairWorkerNoCompiledHairClasses" : detected.error;
        return result;
    }
    std::string managedError;
    auto exporter = [&](int first) {
        const auto state = bl::csharp::exportManagedHairManifest(
            probeAssembly, compilerAssembly, compiledModAssembly,
            modUid, modRoot, privateWorkDir, first);
        if (state.phase != "hair-manifest-prepared") {
            managedError = state.error.empty()
                ? "HairWorkerMonoVMExportFailed" : state.error;
            return -1;
        }
        return state.classes;
    };
    WorkerHairResult result = PrepareOneCompiledHairMod(
        startup, modUid, detected.classes, privateWorkDir,
        exporter, assetExists);
    if (!managedError.empty() && !result.ready)
        result.error = managedError;
    return result;
}

} // namespace bl::csharp::hair
