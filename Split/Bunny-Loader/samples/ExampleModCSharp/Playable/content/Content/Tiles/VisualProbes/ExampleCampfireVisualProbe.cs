using System;
using BunnyLoader.Source;
namespace ExampleModCSharp.Tiles.VisualProbes;
public sealed class ExampleCampfireVisualProbe : BunnyTile
{
    public override void SetStaticDefaults()
    {
        Texture="Content/Tiles/VisualProbes/ExampleCampfireVisualProbe.png";
        DisplayName="Example Campfire Visual Probe (3x2)";
        Flags=BunnyTileFlags.Campfire|BunnyTileFlags.FrameImportant|BunnyTileFlags.Lighted|
              BunnyTileFlags.HasOutlines|BunnyTileFlags.WaterDeath|BunnyTileFlags.LavaDeath;
        CacheDrawData=false;
        Shape=BunnyTileShape.Style3x2;
        FrameToggle=BunnyTileFrameToggle.CampfireVertical;
        AnimationFrameHeight=36;
        AnimationFrames=8;
        AnimationFrameTicks=4;
        AddMapEntry(254,121,2,"Campfire Visual Probe");
    }
    public override void ModifyLight(int x,int y,ref float r,ref float g,ref float b)
    {
        float pulse=0.14f+(Environment.TickCount64/200L%14L)*0.005f;
        r=0.1f+pulse;g=0.9f+pulse;b=0.3f+pulse;
    }
    public override void DrawEffects(int x,int y,int frameX,int frameY,
                                     BunnyTileDrawContext draw)
    {
        if(frameY>=36)return;
        int frame=draw.AnimationFrame;
        if(frame<0)return;
        draw.DrawFlameTexture(frameX,frameY+frame*36,16,16,0,2,0x00ffffffu);
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
