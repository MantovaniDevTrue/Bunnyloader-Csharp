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
    private readonly Dust[] particles = new Dust[2];
    private int particleCount;
    private bool specialRequested;
    private bool specialPhase;
    private int animationFrame=-1;

    private readonly struct Sprite {
        public readonly int FrameX, FrameY, Width, Height, OffsetX, OffsetY, Effects;
        public readonly uint Color;
        public readonly int Layer, Phase;
        public Sprite(int frameX, int frameY, int width, int height, int offsetX,
                      int offsetY, uint color, int effects,int layer,int phase) {
            FrameX=frameX; FrameY=frameY; Width=width; Height=height;
            OffsetX=offsetX; OffsetY=offsetY; Color=color; Effects=effects; Layer=layer; Phase=phase;
        }
    }

    private readonly struct Dust {
        public readonly int Type, OffsetX, OffsetY, Width, Height, SpeedX, SpeedY, Alpha, Scale;
        public readonly uint Argb;
        public readonly int Styled, NoGravity, FadeInMilli, VelocityXScaleMilli;
        public readonly int VelocityYScaleMilli, VelocityYAddMilli, PositionXAdd, AlphaAdd;
        public Dust(int type,int offsetX,int offsetY,int width,int height,
                    int speedX,int speedY,int alpha,int scale,uint argb,
                    int styled,int noGravity,int fadeInMilli,int velocityXScaleMilli,
                    int velocityYScaleMilli,int velocityYAddMilli,int positionXAdd,int alphaAdd) {
            Type=type;OffsetX=offsetX;OffsetY=offsetY;Width=width;Height=height;
            SpeedX=speedX;SpeedY=speedY;Alpha=alpha;Scale=scale;Argb=argb;
            Styled=styled;NoGravity=noGravity;FadeInMilli=fadeInMilli;
            VelocityXScaleMilli=velocityXScaleMilli;VelocityYScaleMilli=velocityYScaleMilli;
            VelocityYAddMilli=velocityYAddMilli;PositionXAdd=positionXAdd;AlphaAdd=alphaAdd;
        }
    }

    internal BunnyTileDrawContext(int tileType, int cellX, int cellY) {
        Reset(tileType,cellX,cellY);
    }

    internal void Reset(int tileType,int cellX,int cellY)
        => Reset(tileType,cellX,cellY,-1);

    internal void Reset(int tileType,int cellX,int cellY,int frame) {
        if(tileType < 754 || tileType > 65535 || cellX < 0 || cellY < 0 ||
           cellX > 100000 || cellY > 100000 || frame < -1 || frame > 127)
            throw new ArgumentOutOfRangeException(nameof(tileType));
        type=tileType; x=cellX; y=cellY; count=0;animationFrame=frame;
        specialRequested=false; specialPhase=false;particleCount=0;
    }

    public int AnimationFrame => animationFrame;
    public int Count => count;
    public int ParticleCount => particleCount;

    public void EmitDust(int dustType,int offsetX,int offsetY,int width,int height,
                         int speedXMilli=0,int speedYMilli=0,int alpha=100,
                         int scaleMilli=1000,uint argb=0xffffffffu) {
        AddDust(dustType,offsetX,offsetY,width,height,speedXMilli,speedYMilli,
                alpha,scaleMilli,argb,0,0,0,1000,1000,0,0,0);
    }

    public void EmitDustStyled(int dustType,int offsetX,int offsetY,int width,int height,
                               int speedXMilli,int speedYMilli,int alpha,int scaleMilli,uint argb,
                               bool noGravity,int fadeInMilli,int velocityXScaleMilli,
                               int velocityYScaleMilli,int velocityYAddMilli,
                               int positionXAdd,int alphaAdd) {
        AddDust(dustType,offsetX,offsetY,width,height,speedXMilli,speedYMilli,
                alpha,scaleMilli,argb,1,noGravity?1:0,fadeInMilli,
                velocityXScaleMilli,velocityYScaleMilli,velocityYAddMilli,
                positionXAdd,alphaAdd);
    }

    private void AddDust(int dustType,int offsetX,int offsetY,int width,int height,
                         int speedXMilli,int speedYMilli,int alpha,int scaleMilli,uint argb,
                         int styled,int noGravity,int fadeInMilli,int velocityXScaleMilli,
                         int velocityYScaleMilli,int velocityYAddMilli,
                         int positionXAdd,int alphaAdd) {
        if (particleCount>=particles.Length)
            throw new InvalidOperationException("Tile particle limit exceeded");
        if (dustType<0||dustType>4095||offsetX < -64||offsetX>64||
            offsetY < -64||offsetY>64||width<1||width>32||height<1||height>32||
            speedXMilli < -10000||speedXMilli>10000||
            speedYMilli < -10000||speedYMilli>10000||
            alpha<0||alpha>255||scaleMilli<100||scaleMilli>3000||
            styled<0||styled>1||noGravity<0||noGravity>1||
            fadeInMilli<0||fadeInMilli>3000||
            velocityXScaleMilli<0||velocityXScaleMilli>3000||
            velocityYScaleMilli<0||velocityYScaleMilli>3000||
            velocityYAddMilli < -10000||velocityYAddMilli>10000||
            positionXAdd < -32||positionXAdd>32||alphaAdd<0||alphaAdd>100)
            throw new ArgumentOutOfRangeException(nameof(dustType));
        particles[particleCount++]=new Dust(dustType,offsetX,offsetY,width,height,
                                           speedXMilli,speedYMilli,alpha,scaleMilli,argb,
                                           styled,noGravity,fadeInMilli,velocityXScaleMilli,
                                           velocityYScaleMilli,velocityYAddMilli,positionXAdd,alphaAdd);
    }

    internal string EncodeParticles() {
        var result=new StringBuilder("PARTICLES|");
        result.Append(particleCount.ToString(CultureInfo.InvariantCulture));
        for (int i=0;i<particleCount;i++) {
            var p=particles[i];
            result.Append('|').Append(type).Append(',').Append(x).Append(',').Append(y)
                .Append(',').Append(p.Type).Append(',').Append(p.OffsetX)
                .Append(',').Append(p.OffsetY).Append(',').Append(p.Width)
                .Append(',').Append(p.Height).Append(',').Append(p.SpeedX)
                .Append(',').Append(p.SpeedY).Append(',').Append(p.Alpha)
                .Append(',').Append(p.Scale).Append(',')
                .Append(p.Argb.ToString(CultureInfo.InvariantCulture));
            if(p.Styled!=0)
                result.Append(',').Append(p.NoGravity)
                    .Append(',').Append(p.FadeInMilli)
                    .Append(',').Append(p.VelocityXScaleMilli)
                    .Append(',').Append(p.VelocityYScaleMilli)
                    .Append(',').Append(p.VelocityYAddMilli)
                    .Append(',').Append(p.PositionXAdd)
                    .Append(',').Append(p.AlphaAdd);
        }
        return result.ToString();
    }

    public void RequestSpecialDraw() => specialRequested=true;

    internal bool SpecialRequested => specialRequested;
    internal void BeginSpecialDraw() => specialPhase=true;

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
        sprites[count++]=new Sprite(frameX,frameY,width,height,offsetX,offsetY,argb,effects,layer,specialPhase?1:0);
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
            if(s.Layer!=0 || s.Phase!=0)result.Append(',').Append(s.Layer);
            if(s.Phase!=0)result.Append(',').Append(s.Phase);
        }
        return result.ToString();
    }
}
