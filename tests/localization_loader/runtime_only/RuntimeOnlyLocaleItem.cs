using BunnyLoader.Source;

namespace BunnyLoader.LocalizationRuntimeOnlyProbe;

public sealed class RuntimeOnlyLocaleItem : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Runtime-only localization";
        Texture = "Content/Items/RuntimeOnlyLocaleItem.png";
        MaxStack = 1;
    }

    [System.Runtime.CompilerServices.MethodImpl(System.Runtime.CompilerServices.MethodImplOptions.NoInlining)]
    public static void InstallTranslation()
    {
        BunnyLocalizationRuntime.Register("LegacyInterface.28", "en-US", "Runtime only");
        System.GC.KeepAlive(typeof(RuntimeOnlyLocaleItem));
    }
}
