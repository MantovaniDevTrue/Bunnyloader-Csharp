using System;
using BunnyLoader.Source;

namespace ExampleModCSharp.Biomes;

// Port integral do estilo original: longe, meio animado em quatro imagens e
// frente com parallax. Quem decide quando ativar o estilo é o sistema de cena;
// o comando ExampleSurfaceBackgroundCommand permite testar este loader isolado.
public sealed class ExampleSurfaceBackgroundStyle : BunnySurfaceBackgroundStyle
{
    private const string Backgrounds="Assets/Textures/Backgrounds/";
    private int far,close;
    private readonly int[] middle=new int[4];
    private static int frameCounter,frame;
    public override void SetStaticDefaults()
    {
        // ModId é atribuído pelo carregador. O exemplo não conhece GUIDs fixos
        // nem grava IDs em saves; resolve as seis imagens uma vez por sessão.
        far=BunnyBackgroundTextures.GetSlot(ModId,Backgrounds+"ExampleBiomeSurfaceFar");
        close=BunnyBackgroundTextures.GetSlot(ModId,Backgrounds+"ExampleBiomeSurfaceClose");
        for(int i=0;i<3;i++)middle[i]=BunnyBackgroundTextures.GetSlot(ModId,Backgrounds+"ExampleBiomeSurfaceMid"+i);
        // Também demonstra o caminho completo, equivalente ao usado no JS.
        // O UID vem do runtime para que uma cópia em outro mod continue válida.
        middle[3]=BunnyBackgroundTextures.GetSlot(ModId+"/"+Backgrounds+"ExampleBiomeSurfaceMid3");
    }
    public override void ModifyFarFades(Span<float> fades,double transitionSpeed)
    {
        // O original mantém as montanhas antigas desaparecendo enquanto estas
        // surgem. Calcular em Double antes do store preserva o arredondamento JS.
        for(int i=0;i<fades.Length;i++)fades[i]=(float)(i==Slot
            ?Math.Min(1,(double)fades[i]+transitionSpeed)
            :Math.Max(0,(double)fades[i]-transitionSpeed));
    }
    public override int ChooseFarTexture()=>far;
    public override int ChooseMiddleTexture()
    {
        // O original avança depois de 13 chamadas efetivas, não por tick do
        // jogador. Isso conserva a animação durante transições e no menu.
        if(++frameCounter>12){frame=(frame+1)%4;frameCounter=0;}
        return middle[frame];
    }
    public override int ChooseCloseTexture(ref double scale,ref double parallax,ref double a,ref double b)
    {
        // Os defaults 1.25/.37/1800/1750 já chegam por referência. Alterar esses
        // valores muda escala, movimento horizontal e altura de qualquer estilo.
        return close;
    }
}
