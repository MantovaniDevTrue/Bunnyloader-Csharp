namespace LightsAndShadowsCSharp;

public struct LightingFrame
{
    public bool DayTime;
    public double GameTime;
    public double WorldSurface;
    public float ScreenX;
    public float ScreenY;
    public int ScreenWidth;
    public int ScreenHeight;
    public float PlayerCenterY;
    public bool InvertedGravity;
    public int MoonPhase;
    public byte SkyRed;
    public byte SkyGreen;
    public byte SkyBlue;
    public bool Snow;
    public bool Crimson;
    public bool Corrupt;
    public int SnowBackground;
    public int DesertBackground;
    public float SnowBackgroundAlpha;
    public float DesertBackgroundAlpha;
}

public readonly struct LightingResult
{
    public readonly float SunX;
    public readonly float SunY;
    public readonly float GradientClock;
    public readonly float SunOrMoonIntensity;
    public readonly float ShadowThreshold;
    public readonly bool SunEnabled;
    public readonly bool DayTime;

    public LightingResult(float sunX, float sunY, float gradientClock,
        float sunOrMoonIntensity, float shadowThreshold, bool sunEnabled, bool dayTime)
    {
        SunX = sunX;
        SunY = sunY;
        GradientClock = gradientClock;
        SunOrMoonIntensity = sunOrMoonIntensity;
        ShadowThreshold = shadowThreshold;
        SunEnabled = sunEnabled;
        DayTime = dayTime;
    }
}
