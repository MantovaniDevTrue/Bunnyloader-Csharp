using System;

namespace LightsAndShadowsCSharp;

public static class LightingCalculator
{
    public static LightingResult Compute(in LightingFrame frame, LightingSettings settings)
    {
        settings.Validate();
        float width = Math.Max(1, frame.ScreenWidth);
        float height = Math.Max(1, frame.ScreenHeight);
        float surface = (float)frame.WorldSurface;
        float surfacePixels = surface * 16f;
        float progress = (float)frame.GameTime / (frame.DayTime ? 27000f : 16200f);
        float inverse = 1f - progress;
        float sunX = (progress * width * 0.5f - 100f * inverse) / width;
        float denominator = surfacePixels - 600f;
        float cameraCorrection = Math.Abs(denominator) > 0.0001f
            ? -frame.ScreenY / denominator * 200f : 0f;
        float sunY = cameraCorrection + inverse * inverse * 250f + 180f;
        if (frame.InvertedGravity) sunY = height - sunY;

        float sky = (1f - 1.2f * frame.SkyRed * 0.3f +
            frame.SkyGreen * 0.6f + frame.SkyBlue * 0.1f) / 255f;
        if (frame.Snow && !frame.Crimson && !frame.Corrupt)
            sky -= frame.SnowBackgroundAlpha * 0.1f;
        if (frame.Crimson) sky += 0.3f;
        if (frame.SnowBackground == 263 || frame.SnowBackground == 258 || frame.SnowBackground == 267)
            sky -= frame.SnowBackgroundAlpha;
        if (frame.SnowBackground == 263)
            sky -= frame.SnowBackgroundAlpha * 0.5f;
        if (frame.DesertBackground == 248)
            sky -= frame.DesertBackgroundAlpha * 0.9f;

        float moon = frame.MoonPhase == 0 ? 1.01f
            : frame.MoonPhase == 3 || frame.MoonPhase == 5 ? 0.9f
            : frame.MoonPhase == 4 ? 0.6f : 1f;
        float daylight = 0.8f * settings.LightIntensity * (1f + sky * 0.4f);
        float depth = frame.PlayerCenterY / 16f - (surface - 150f);
        if (depth > 0f)
        {
            moon *= 1f - depth / 200f;
            daylight *= 1f - depth / 600f;
        }

        float shadowThreshold = frame.DesertBackground == 248
            ? 1f : 1f - frame.DesertBackgroundAlpha * 0.09f;
        if (!frame.DayTime) shadowThreshold = 0.02f;
        float light = frame.DayTime ? daylight : 0.8f * settings.MoonlightIntensity * moon;
        float clock = (float)frame.GameTime / (frame.DayTime ? 54000f : 32400f);
        bool aboveSurface = frame.PlayerCenterY < surfacePixels + 800f;

        return new LightingResult(sunX, sunY / height, clock,
            settings.UseLight ? Math.Max(0f, light) : 0f,
            shadowThreshold, aboveSurface && settings.UseLight, frame.DayTime);
    }
}
