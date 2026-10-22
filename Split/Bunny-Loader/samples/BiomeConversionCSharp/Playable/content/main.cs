using BunnyLoader.Source;

namespace BiomeConversionExample;

// Categoria concreta baseada em ModBiomeConversion do tModLoader. Seu Type identifica
// UMA CONVERSÃO, não um tile ou bioma. Não some esse número aos TileID/WallID.
public sealed class ExampleStoneConversion : ModBiomeConversion
{
    public override void PostSetupContent()
    {
        // Todos os SetStaticDefaults do mod já terminaram. Esta extensão Bunny publica
        // uma tabela nativa; não chama C# para cada tile e não usa JavaScript.
        RegisterTileConversion(0, 1); // Dirt -> Stone (TileID vanilla).
        RegisterWallConversion(2, 1); // DirtUnsafe -> Stone (WallID vanilla).
        // Não substitui tiles inativos nem cria paredes onde WallID=0.
        // Não existe propagação automática: somente a área solicitada é convertida.
    }
}

public sealed class ConversionWand : BunnyWeapon
{
    public override void SetDefaults()
    {
        DisplayName = "Stone Conversion Wand";
        Texture = "Content/Items/Weapons/ConversionWand";
        Damage = 1;
        UseTime = 30;
        ShootSpeed = 8f;
        Shoot = nameof(ConversionBolt);
    }
}

public sealed class ConversionBolt : BunnyProjectile
{
    private bool requested;
    public override void SetDefaults()
    {
        DisplayName = "Stone Conversion Bolt";
        Texture = "Content/Projectiles/ConversionBolt";
        Width = 10; Height = 10; AiStyle = 1; Friendly = true;
        Penetrate = 1; TimeLeft = 90;
    }
    public override bool AI(ref BunnyProjectileAiState state)
    {
        // Apenas uma solicitação por projétil, após ele avançar. O worker usa um
        // snapshot: converter X/Y de pixels para coordenadas de TILE (16 pixels).
        // Este método é chamado pelo dispatcher real de IA, não pelo compilador.
        if (!requested && state.Age >= 12)
        {
            var conversion = ModContent.GetInstance<ExampleStoneConversion>();
            // True = pedido admitido. O jogo aplica na próxima atualização, até 4
            // pedidos por frame. Sair do mundo/descarregar o mod cancela a fila.
            // Testar em cópia de mundo: a conversão altera o terreno e o save normal
            // do Terraria preserva o resultado. Multiplayer é recusado explicitamente.
            requested = conversion?.Convert((int)(state.X / 16), (int)(state.Y / 16), 2) == true;
            // Se a fila estiver cheia, tenta de novo no próximo callback de IA.
        }
        return false; // Preserva a IA/movimento vanilla deste projétil.
    }
}
