const { NPCID, SoundID, ItemID } = Terraria.ID;

export class NativePhaseSlime extends ModNPC {
    SetStaticDefaults() {
        Terraria.Main.npcFrameCount[this.Type] = 2;
    }

    SetDefaults() {
        this.NPC.width = 32;
        this.NPC.height = 24;
        this.NPC.aiStyle = 0;
        this.NPC.damage = 14;
        this.NPC.defense = 5;
        this.NPC.lifeMax = 90;
        this.NPC.knockBackResist = 0.7;
        this.NPC.HitSound = SoundID.NPCHit1;
        this.NPC.DeathSound = SoundID.NPCDeath1;
        this.NPC.value = ModNPC.NPCValue(0, 0, 0, 30);
        this.AnimationType = NPCID.BlueSlime;
    }
}
