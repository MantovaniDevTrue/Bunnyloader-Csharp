using System;
using BunnyLoader.Source;
namespace ExampleModCSharp.Tiles.VisualProbes;
public sealed class ExampleCampfireVisualProbe : BunnyTile
{
    public override void SetStaticDefaults()
    {
        Texture="Content/Tiles/VisualProbes/ExampleCampfireVisualProbe.png";
        DisplayName="Example Campfire Visual Probe (1x1)";
        Flags=BunnyTileFlags.FrameImportant|BunnyTileFlags.Lighted|
              BunnyTileFlags.NoAttach|BunnyTileFlags.WaterDeath|BunnyTileFlags.LavaDeath;
        CacheDrawData=false;
        AddMapEntry(254,121,2,"Campfire Visual Probe");
    }
    public override void ModifyLight(int x,int y,ref float r,ref float g,ref float b)
    {
        float pulse=(Environment.TickCount64/200L%6L)*0.01f;
        r=0.18f+pulse;g=0.98f+pulse;b=0.38f+pulse;
    }
    public override void PostDraw(int x,int y,BunnyTileDrawContext draw)
    {
        int frame=(int)((Environment.TickCount64/68L)%8L);
        draw.DrawFlameTexture(0,frame*36,16,16,0,2,0x00ffffffu);
    }
    public override void EmitParticles(int x,int y,int frameX,int frameY,BunnyTileDrawContext draw)
    {
        if(frameY!=0||Random.Shared.Next(3)!=0)return;
        draw.EmitDustStyled(31,2,-4,4,8,0,0,100,1000,0xffffffffu,
                            false,500+Random.Shared.Next(10)*100,200,200,
                            -500-Random.Shared.Next(10)*100,
                            Random.Shared.Next(8),Random.Shared.Next(100));
    }
}
