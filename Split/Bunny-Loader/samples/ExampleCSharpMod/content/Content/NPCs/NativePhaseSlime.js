const { NPCID, SoundID, ItemID } = Terraria.ID;

const NewProjectile = Terraria.Projectile['int NewProjectile(IEntitySource spawnSource, Vector2 position, Vector2 velocity, int Type, int Damage, float KnockBack, int Owner, float ai0, float ai1, float ai2, NewProjectileModifier modifer)'];

export class NativePhaseSlime extends ModNPC {
    SetStaticDefaults() {
        Terraria.Main.npcFrameCount[this.Type] = 2;
    }

    AI(npc) {
        if (Terraria.Main.netMode !== 0) return;
        const attack = bl.csharp.npcAttack(npc.whoAmI, npc.type);
        if (!attack) return;
        const shotType = ModProjectile.getTypeByName('NativeSlimeOrb');
        if (shotType < 0) return;
        const source = npc.GetSource_FromAI();
        const spawn = Vector2.new(attack.x, attack.y);
        const owner = Terraria.Main.myPlayer;
        NewProjectile(source, spawn, Vector2.new(attack.vx0, attack.vy0), shotType,
                      attack.damage, 0, owner, 0, 0, 0, null);
        if (attack.count === 2) {
            NewProjectile(source, spawn, Vector2.new(attack.vx1, attack.vy1), shotType,
                          attack.damage, 0, owner, 0, 0, 0, null);
        }
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
