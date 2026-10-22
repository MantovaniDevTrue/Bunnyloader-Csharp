using System;
using BunnyLoader.Source;

namespace ExampleModCSharp.Biomes;

// Port do FillTextureArray original. O serviço nativo cuida dos getters do
// Terraria, do estilo anterior e dos frames de transição, sem desenhar por JS.
public sealed class ExampleUndergroundBackgroundStyle : BunnyUndergroundBackgroundStyle
{
    private readonly int[] slots=new int[4];
    public override void SetStaticDefaults()
    {
        // ModId vem do carregador: outro mod pode reutilizar esta classe sem
        // editar o núcleo. IDs são válidos só nesta sessão; nunca grave no save.
        // Resolver uma vez evita strings e quatro chamadas nativas por desenho.
        for(int i=0;i<slots.Length;i++)
            slots[i]=BunnyBackgroundTextures.GetSlot(ModId,
                "Assets/Textures/Backgrounds/ExampleBiomeUnderground"+i);
    }
    public override void FillTextureArray(Span<int> textureSlots)
    {
        // 0: borda céu/terra; 1: terra; 2: borda terra/pedra; 3: pedra.
        // A quinta camada continua 6, como no original: transição para o inferno.
        for(int i=0;i<4;i++)
            textureSlots[i]=slots[i];
    }
}
