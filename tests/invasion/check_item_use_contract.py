from pathlib import Path

root = Path(__file__).resolve().parents[2]
native = (root / "app/src/main/cpp/experiments/csharp/NativeCSharpItemUse.cpp").read_text()
auto = (root / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()
sdk = (root / "experiments/csharp/BunnyCsSdk/BunnyItem.cs").read_text()
dispatcher = (root / "experiments/csharp/BunnyCsSdk/BunnyEntitySourceDispatcher.cs").read_text()
beacon = (root / "samples/ExampleModCSharp/Playable/content/Content/Items/Consumables/ExampleInvasionBeacon.cs").read_text()
transport = (root / "experiments/csharp/BunnyCsSdk/BunnyNativeItemUse.cs").read_text()

required = [
    'parseSignature("void ItemCheck_StartActualUse(Item sItem)")',
    'runtime::modItemTypeByKey(binding.mod + "/" + binding.name)',
    'worldIsAuthoritative(mode)',
    'runtime::sendToServer("invuse "',
    'enqueueVerifiedRemoteItemUse(',
    'g_events.size() >= kEventsLimit',
    'g_requests.size() >= kBindingsLimit',
    'bunny_item_use_bind(',
    'bunny_item_use_poll(',
    'bunny_item_use_poll_context(',
    'startFlags(player)',
    'bunny_item_use_ready(',
    'if (g_originalUse) g_originalUse(player, item, method);',
]
for marker in required:
    assert marker in native, marker
assert native.index('itemType = runtime::field<int32_t>(item, g_itemTypeOffset);') < native.index('if (g_originalUse) g_originalUse(')
assert 'setNativeItemUseWorldState(' in auto
assert 'pumpNativeItemUseBindingsOnGameThread();' in auto
assert 'public virtual bool OnUse(int playerIndex) => false;' in sdk
assert 'typeof(BunnyItem).IsAssignableFrom(type)' in dispatcher
assert 'pair.Value.OnUse(usingPlayer, useContext);' in dispatcher
assert 'BunnyNativeItemUse.TryPollContext(' in dispatcher
assert 'BunnyNativeItemUse.IsReady(' in dispatcher
assert 'BunnyNativeItemUse.Bind(' in dispatcher
assert 'public override bool OnUse(int playerIndex)' in beacon
assert 'public override bool OnUse(int playerIndex, BunnyUseContext context)' in beacon
assert 'context.CanStartSurfaceInvasion && ExampleInvasion.Start(true)' in beacon
assert 'downedBoss1' in native
assert 'FirstBossDefeated' in (root / 'experiments/csharp/BunnyCsSdk/BunnyUseContext.cs').read_text()
assert 'public virtual bool OnUse(int playerIndex, BunnyUseContext context)' in sdk
assert 'ExampleInvasion.Start(true)' in beacon
assert 'CreateRecipe()' in beacon and 'AddTile(BunnyVanillaTiles.WorkBenches)' in beacon
assert 'Consumable = false;' in beacon
assert 'EntryPoint = "bunny_item_use_bind"' in transport
assert 'EntryPoint = "bunny_item_use_poll"' in transport
assert 'EntryPoint = "bunny_item_use_poll_context"' in transport
assert 'EntryPoint = "bunny_item_use_ready"' in transport
assert 'QuickJS' not in native
assert 'api.method_get_param(candidate, 0), "Terraria.Item"' in native
assert 'api.method_get_return_type(candidate), "System.Void"' in native
assert 'g_requests.push_back(std::move(binding));' in native
assert 'Stage=Bind Class=%s Type=%d Result=Ready' in native
assert 'Stage=ActualUse Class=%s Player=%d Flags=%d Result=Queued' in native
assert 'Stage=Consumed Class=%s Player=%d Flags=%d Result=DeliveredToMonoVM' in native
assert 'int flags = g_ready.load(std::memory_order_acquire)' in native
assert 'if (health >= 0 && runtime::field<int32_t>(player, health) > 0)' in native
assert 'isStaticType(surface, "System.Single")' in native
assert 'isStaticType(surface, "System.Double")' in native
assert 'if (!day || !surface || !invasion || !menu || health < 0) return 0;' not in native
assert 'BunnyDustEffects.EmitAtPlayer<ExampleSparkDust>' in (
    root / "samples/ExampleModCSharp/Playable/content/Content/Items/ExampleDustCharm.cs"
).read_text()
dust = (root / "app/src/main/cpp/experiments/csharp/NativeDustEmission.cpp").read_text()
assert '"Microsoft.Xna.Framework.Color"' in dust
assert 'Stage=EmitRequest Class=%s Player=%d Count=%d Result=Queued' in dust
# Android 301720 uses Graphics.Color (verified in the runtime metadata).
assert '"Microsoft.Xna.Framework.Graphics.Color"' in dust
assert 'api.class_is_valuetype(color)' in dust and 'api.class_value_size(color,&align)==4' in dust
print("Native CSharp item-use hook, dust emission and invasion invoker: OK")

managed = (root / "app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp").read_text()
for label in ("FirstWorldTick", "OnUseResult", "WorldTick Result=Error"):
    assert "Stage=" + label in managed, label
assert 'void reportManagedItemUse(int level, const std::string& message)' in managed
assert 'dlsym(bunny, "bunny_native_forward_log")' in managed
assert 'reportManagedItemUse(ANDROID_LOG_WARN,' in managed
assert 'reportManagedItemUse(ANDROID_LOG_INFO,' in managed
assert 'core/Log.h' not in managed
assert 'bunny_native_forward_log' in (root / 'app/src/main/cpp/core/Log.cpp').read_text()
assert 'owner.firstWorldUseReported = true;' in managed
assert 'owner.firstWorldErrorReported = true;' in managed
assert 'catch (Exception error)' in dispatcher
assert 'context.LastError = "OnUse(" + pair.Key + "): "' in dispatcher
assert '" Applied=0 Error=" + error.GetType().Name' in dispatcher
assert 'context.Faults++;' in dispatcher
assert dispatcher.index('SaveInvasionIfChanged(context);', dispatcher.index('bool applied = pair.Value.OnUse')) > dispatcher.index('catch (Exception error)', dispatcher.index('bool applied = pair.Value.OnUse'))
assert 'return "lastUse=" + context.LastUse +' in dispatcher
assert 'outcome.find(" Error=")' in managed
assert 'outcome.find(" Error=") != std::string::npos' in managed
print("CSHARP_ITEM_USE_SESSION_LOG_GUARD=PASS")

assert 'bool hasDust = false;' in dispatcher
assert 'typeof(BunnyDust).IsAssignableFrom(type)' in dispatcher
assert 'BunnyDustSourceDispatcher.RegisterLoaded(modId, assembly) < 0' in dispatcher
assert dispatcher.index('BunnyDustSourceDispatcher.RegisterLoaded(modId, assembly)') < dispatcher.index('Contexts.Add(context);')
assert 'Stage=MonoBind Result=Rejected Reason=ManagedRegisterOrBindFailed' in managed
entity_test = (root / "tests/entity_source/Program.cs").read_text()
assert 'BunnyDust assembly registered before item use' in entity_test
assert 'Dust registration remains idempotent' in entity_test
print("CSHARP_DUST_EARLY_REGISTRATION_GUARD=PASS")

assert 'public virtual string OnUseRejectionReason(int playerIndex, BunnyUseContext context)' in sdk
assert 'pair.Value.OnUseRejectionReason(usingPlayer, useContext)' in dispatcher
assert '" Reason=" + reason' in dispatcher
assert '"Nighttime"' in beacon and '"NotAtSurface"' in beacon
assert '"VanillaInvasionActive"' in beacon and '"AlreadyRunning"' in beacon
assert 'std::string lastUseOutcome;' in managed
assert 'int useReports = 0;' in managed
assert 'needsUseStatus = g_entityBindings[static_cast<size_t>(job.binding)].useReports < 12;' in managed
assert 'owner.lastUseOutcome != outcome' in managed
assert 'const size_t end = details.find(" npc=", use);' in managed
assert 'binding.lastUseOutcome.clear();' in managed
assert 'binding.useReports = 0;' in managed
host = (root / "tests/invasion/DispatcherIntegration.cs").read_text()
assert '"nighttime prevents start with explicit reason"' in host
assert '"valid daytime surface use starts invasion"' in host
assert '"duplicate use refuses without resetting progress"' in host
print("CSHARP_INVASION_BEACON_USE_FEEDBACK_GUARD=PASS")

dust = (root / "experiments/csharp/BunnyCsSdk/BunnyDustSourceDispatcher.cs").read_text()
assert "BunnyModAssemblyRegistry.Acquire(modId, assemblyPath)" in dust
assert "RegisterLoaded(modId, lease.Assembly)" in dust
assert "BunnyModAssemblyRegistry.TryResolve(assembly, out _, out string resolved)" in dust
assert "BunnyEntitySourceDispatcher.TryRegisterDustAssembly(assembly)" in dust
assert "RegistrationError=" in dust
assert "BunnyDustSourceDispatcher.RegisterLoaded(modId, assembly)" in dispatcher
assert "internal static bool TryRegisterDustAssembly(Assembly assembly)" in dispatcher
source_runtime = (root / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()
assert "Stage=BindingPlan Mod=%s Registered=%d ManagedHooks=%zu" in source_runtime
print("CSHARP_DUST_ASSEMBLY_RECOVERY_GUARD=PASS")
native_dust = (root / "app/src/main/cpp/experiments/csharp/NativeDustEmission.cpp").read_text()
assert "findMethodBySignature(dust, il2cpp::parseSignature(" in native_dust
assert "int NewDust(Vector2 Position, int Width, int Height, int Type, " in native_dust
assert 'nativeResolutionFailure = "NewDustSignatureUnavailable";' in native_dust
assert 'Stage=NativeContract Detail=%s' in native_dust
assert 'type<runtime::kVanillaDustCount||!resolve()' in native_dust
mailbox = (root / "app/src/main/cpp/experiments/csharp/NativeInvasionMailbox.cpp").read_text()
assert 'Stage=NativeSpawn Result=Rejected Mod=%s NPC=%s Reason=%s' in mailbox
assert 'Stage=NativeSpawn Result=Created Mod=%s NPC=%s Slot=%d' in mailbox
assert 'errorsReported.fill(false);' in mailbox
assert 'spawnErrorName(result.error)' in mailbox
assert '" Invasion=" + snapshot.Phase' in dispatcher
assert '" SpawnFailed=" + invasion.Pipeline.FailedCount' in dispatcher
print("CSHARP_DUST_NATIVE_INVASION_SPAWN_QA_GUARD=PASS")
native_dust = (root / "app/src/main/cpp/experiments/csharp/NativeDustEmission.cpp").read_text()
assert "findMethodBySignature(dust, il2cpp::parseSignature(" in native_dust
assert "int NewDust(Vector2 Position, int Width, int Height, int Type, " in native_dust
assert 'nativeResolutionFailure = "NewDustSignatureUnavailable";' in native_dust
assert 'Stage=NativeContract Detail=%s' in native_dust
assert 'type<runtime::kVanillaDustCount||!resolve()' in native_dust
mailbox = (root / "app/src/main/cpp/experiments/csharp/NativeInvasionMailbox.cpp").read_text()
assert 'Stage=NativeSpawn Result=Rejected Mod=%s NPC=%s Reason=%s' in mailbox
assert 'Stage=NativeSpawn Result=Created Mod=%s NPC=%s Slot=%d' in mailbox
assert 'errorsReported.fill(false);' in mailbox
assert 'spawnErrorName(result.error)' in mailbox
assert '" Invasion=" + snapshot.Phase' in dispatcher
assert '" SpawnFailed=" + invasion.Pipeline.FailedCount' in dispatcher
print("CSHARP_DUST_NATIVE_INVASION_SPAWN_QA_GUARD=PASS")
