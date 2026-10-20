using System;
using BunnyLoader.Source;
namespace ExampleModCSharp.Tiles.VisualProbes;
public sealed class ExampleLampVisualProbe : BunnyTile
{
    public override void SetStaticDefaults()
    {
        Texture="Content/Tiles/VisualProbes/ExampleLampVisualProbe.png";
        DisplayName="Example Lamp Visual Probe (1x3)";
        Flags=BunnyTileFlags.FrameImportant|BunnyTileFlags.Lighted|
              BunnyTileFlags.NoAttach|BunnyTileFlags.WaterDeath|BunnyTileFlags.LavaDeath;
        CacheDrawData=false;
        Shape=BunnyTileShape.Style1x3;
        FrameToggle=BunnyTileFrameToggle.LampHorizontal;
        AddMapEntry(253,221,3,"Lamp Visual Probe");
    }
    public override void SetSpriteEffects(int x,int y,ref int effects)
    {
        if(x%2==0)effects=1;
    }
    public override void ModifyLight(int x,int y,ref float r,ref float g,ref float b)
    {
        r=1f;g=0.75f;b=1f;
    }
    public override void DrawEffects(int x,int y,int frameX,int frameY,BunnyTileDrawContext draw)
    {
        if(frameX==0&&frameY==0)draw.RequestSpecialDraw();
    }
    public override void SpecialDraw(int x,int y,BunnyTileDrawContext draw)
    {
        int effects=x%2==0?1:0;
        for(int i=0;i<7;i++)
            draw.DrawFlameTexture(0,0,16,16,Random.Shared.Next(-2,3),
                                  Random.Shared.Next(-2,3)+2,0x00646464u,effects);
    }
    public override void EmitParticles(int x,int y,int frameX,int frameY,BunnyTileDrawContext draw)
    {
        if(frameX!=0||frameY!=0||Random.Shared.Next(40)!=0)return;
        draw.EmitDustStyled(21,4,2,4,4,0,0,100,1000,0xffffffffu,
                            Random.Shared.Next(3)!=0,0,300,300,-1500,0,0);
    }
}
