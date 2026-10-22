// Test-only host for the byte-identical, pinned upstream conversion classes.
namespace Microsoft.Xna.Framework { internal class ReferenceAnchor {} }
namespace Microsoft.Xna.Framework.Graphics { internal class ReferenceAnchor {} }
namespace Terraria.DataStructures { internal class ReferenceAnchor {} }
namespace Terraria.GameContent { internal class ReferenceAnchor {} }
namespace Terraria.ID { public static class BiomeConversionID { public const int Count=22; } }
namespace Terraria.ModLoader {
    public sealed class CloneByReferenceAttribute : System.Attribute {}
    public static class ModTypeLookup<T> { public static void Register(T value){} }
    public abstract class ModType {
        protected abstract void Register();
        public void RegisterForTest()=>Register();
        public virtual void SetupContent(){}
        public virtual void SetStaticDefaults(){}
    }
}
internal sealed class OriginalConversion : Terraria.ModLoader.ModBiomeConversion {
    public int Defaults,PostSetup;
    public override void SetStaticDefaults()=>Defaults++;
    public override void PostSetupContent()=>PostSetup++;
}
