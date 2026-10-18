using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleMinion : BunnyProjectile
{
    // Imutável e compartilhado: não alocar configurações dentro de AI.
    private static readonly BunnyMinionMovement movement=new(followSpeed:4f,attackSpeed:8f,
        catchUpSpeed:12f,followInertia:80f,attackInertia:20f,followOffsetX:-42f,followOffsetY:-48f);
    public override void SetDefaults()
    {
        DisplayName="Example Minion";
        Texture="Content/Projectiles/ExampleMinion.png";
        Width=18;Height=28;Frames=4;AnimationFrameTicks=5;
        AiStyle=0;Friendly=true;Minion=true;MinionSlots=1f;
        TileCollide=false;IgnoreWater=true;Penetrate=-1;TimeLeft=18000;
        // Busca feita no núcleo, com cache por instância. AI recebe só um snapshot.
        MinionTargetRange=700f;MinionScanTicks=8;
    }
    public override bool AI(ref BunnyProjectileAiState state)
        =>BunnyMinionSteering.FollowOrAttack(ref state,movement);
}
