// A API que os mods enxergam; o resto fica no escopo privado.
Object.assign(globalThis, {
    Mod, ModLoader, ModContent, ModLocalization,
    UIState, UserInterface, GameInterfaceLayer, LegacyGameInterfaceLayer, InterfaceLayerList, InterfaceScaleType,
    ModItem, ModRecipe, TooltipLine, EquipType, EquipLoader, EquipTexture,
    ModPrefix, PrefixCategory, PrefixLoader,
    ModSystem, TagCompound, ModSceneEffect, ModBiome,
    ModSurfaceBackgroundStyle, ModUndergroundBackgroundStyle, BackgroundTextureLoader,
    ModWaterStyle, ModWaterfallStyle,
    ModPlayer, ModBuff, ModTileEntity, ModTile, ModProjectile,
    ModNPC, NPCLoot, NPCSpawnInfo, SpawnPool, SpawnCondition, NPCShop, NPCHappiness, AffectionLevel, ModGore,
    GlobalItem, GlobalNPC, GlobalProjectile, GlobalLoot,
    ModPacket, NetWriter, NetReader,
    SoundStyle, SoundEngine, SoundLimitBehavior, MusicLoader, SceneEffectPriority,
});
