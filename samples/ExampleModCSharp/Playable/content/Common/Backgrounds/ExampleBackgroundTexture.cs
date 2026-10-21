using BunnyLoader.Source;

namespace ExampleModCSharp.Backgrounds;

// A textura original do ExampleSurfaceBackgroundStyle e registrada sem JavaScript.
// PNGs dentro de qualquer pasta Backgrounds/ tambem entram automaticamente;
// a classe demonstra como registrar explicitamente uma imagem de outra pasta.
public sealed class ExampleBackgroundTexture : BunnyBackgroundTexture
{
    public override void SetStaticDefaults()
    {
        // O caminho e relativo a content/. Nao salve o numero do slot: ele pode
        // mudar quando outro mod for ativado. Resolva por UID/caminho ao usar.
        Texture = "Assets/Textures/Backgrounds/ExampleBiomeSurfaceFar";
    }
}
