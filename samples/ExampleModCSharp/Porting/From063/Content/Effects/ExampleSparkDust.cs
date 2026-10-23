using BunnyLoader.Source;

namespace ExampleModCSharp.Content.Effects;

public sealed class ExampleSparkDust : BunnyDust
{
    public override void SetStaticDefaults()
    {
        Texture = "Content/Effects/TrailblazerSpark.png";
        UpdateType = 6;
    }

    public override void OnSpawn(ref BunnyDustState state)
    {
        state.Scale = 1f;
        // A textura é uma imagem 12x14, sem os três frames da folha vanilla.
        // Definir o retângulo evita selecionar uma região fora da imagem.
        state.FrameX = 0;
        state.FrameY = 0;
        state.FrameWidth = 12;
        state.FrameHeight = 14;
    }

    public override bool Update(ref BunnyDustState state)
    {
        state.X += state.VelocityX;
        state.Y += state.VelocityY;
        state.VelocityX *= 0.96f;
        state.VelocityY -= 0.015f;
        state.Scale *= 0.975f;
        if (state.Scale < 0.15f)
            state.Kill();
        return false;
    }

    public override bool GetAlpha(ref BunnyDustState state, ref uint color)
    {
        color = new BunnyColor(255, 191, 64).PackedRgba;
        return true;
    }
}
