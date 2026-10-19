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
