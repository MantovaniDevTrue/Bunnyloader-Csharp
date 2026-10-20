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
assert 'BunnyDustEffects.EmitAtPlayer<ExampleSparkDust>' in (
    root / "samples/ExampleModCSharp/Playable/content/Content/Items/ExampleDustCharm.cs"
).read_text()
dust = (root / "app/src/main/cpp/experiments/csharp/NativeDustEmission.cpp").read_text()
assert '"Microsoft.Xna.Framework.Color"' in dust
assert '"Microsoft.Xna.Framework.Graphics.Color"' not in dust
print("Native CSharp item-use hook, dust emission and invasion invoker: OK")
