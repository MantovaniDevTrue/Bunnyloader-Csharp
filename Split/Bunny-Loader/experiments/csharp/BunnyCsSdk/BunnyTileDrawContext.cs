using System;
using System.Globalization;
using System.Text;

namespace BunnyLoader.Source;

public sealed class BunnyTileDrawContext {
    private int type;
    private int x;
    private int y;
    private readonly Sprite[] sprites = new Sprite[8];
    private int count;

    private readonly struct Sprite {
        public readonly int FrameX, FrameY, Width, Height, OffsetX, OffsetY, Effects;
        public readonly uint Color;
        public readonly int Layer;
        public Sprite(int frameX, int frameY, int width, int height, int offsetX,
                      int offsetY, uint color, int effects,int layer) {
            FrameX=frameX; FrameY=frameY; Width=width; Height=height;
            OffsetX=offsetX; OffsetY=offsetY; Color=color; Effects=effects; Layer=layer;
        }
    }

    internal BunnyTileDrawContext(int tileType, int cellX, int cellY) {
        Reset(tileType,cellX,cellY);
    }

    internal void Reset(int tileType,int cellX,int cellY) {
        if(tileType < 754 || tileType > 65535 || cellX < 0 || cellY < 0 ||
           cellX > 100000 || cellY > 100000)
            throw new ArgumentOutOfRangeException(nameof(tileType));
        type=tileType; x=cellX; y=cellY; count=0;
    }

    public int Count => count;

    public void DrawTileTexture(int frameX,int frameY,int width,int height,
                                int offsetX=0,int offsetY=0,uint argb=0xffffffffu,int effects=0)
        => AddSprite(0,frameX,frameY,width,height,offsetX,offsetY,argb,effects);

    public void DrawGlowTexture(int frameX,int frameY,int width,int height,
                                int offsetX=0,int offsetY=0,uint argb=0xffffffffu,int effects=0)
        => AddSprite(1,frameX,frameY,width,height,offsetX,offsetY,argb,effects);

    public void DrawFlameTexture(int frameX,int frameY,int width,int height,
                                 int offsetX=0,int offsetY=0,uint argb=0xffffffffu,int effects=0)
        => AddSprite(2,frameX,frameY,width,height,offsetX,offsetY,argb,effects);

    private void AddSprite(int layer,int frameX,int frameY,int width,int height,
                           int offsetX,int offsetY,uint argb,int effects) {
        if(count>=sprites.Length)throw new InvalidOperationException("Tile sprite limit exceeded");
        if(frameX<0||frameY<0||frameX>32767||frameY>32767||
           width<1||width>512||height<1||height>512||
           frameX+width>32768||frameY+height>32768||
           offsetX < -256||offsetX>256||offsetY < -256||offsetY>256||
           effects<0||effects>3)
            throw new ArgumentOutOfRangeException(nameof(frameX));
        sprites[count++]=new Sprite(frameX,frameY,width,height,offsetX,offsetY,argb,effects,layer);
    }

    internal string Encode() {
        var result=new StringBuilder("SPRITES|");
        result.Append(count.ToString(CultureInfo.InvariantCulture));
        for(int i=0;i<count;i++) {
            var s=sprites[i];
            result.Append('|').Append(type).Append(',').Append(x).Append(',').Append(y)
                .Append(',').Append(s.FrameX).Append(',').Append(s.FrameY)
                .Append(',').Append(s.Width).Append(',').Append(s.Height)
                .Append(',').Append(s.OffsetX).Append(',').Append(s.OffsetY)
                .Append(',').Append(s.Color.ToString(CultureInfo.InvariantCulture))
                .Append(',').Append(s.Effects);
            if(s.Layer!=0)result.Append(',').Append(s.Layer);
        }
        return result.ToString();
    }
}
