using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;

namespace BunnyLoader.Source;

/// <summary>RGB de iluminação do motor: componentes finitos de 0 a 10.</summary>
public readonly struct BunnyWorldLight {
    public float R { get; }
    public float G { get; }
    public float B { get; }
    public BunnyWorldLight(float r,float g,float b) {
        if(!float.IsFinite(r)||!float.IsFinite(g)||!float.IsFinite(b)||r<0||g<0||b<0||r>10||g>10||b>10)
            throw new ArgumentOutOfRangeException(nameof(r),"World light requires finite RGB in 0..10");
        R=r;G=g;B=b;
    }
    internal string Wire => string.Join(",",R.ToString("R",CultureInfo.InvariantCulture),G.ToString("R",CultureInfo.InvariantCulture),B.ToString("R",CultureInfo.InvariantCulture));
}

/// <summary>Configuração executada no motor, sem uma chamada Mono por parede por quadro.</summary>
public abstract class BunnyWall {
    public string Texture { get; set; } = "";
    public string DisplayName { get; set; } = "";
    public bool Housing { get; set; }
    /// <summary>Campo de WallID, resolvido pelo núcleo; vazio preserva o blend próprio.</summary>
    public string BlendWallName { get; set; } = "";
    public string DustName { get; set; } = "";
    public int FailedDustCount { get; set; } = 1;
    public int DustCount { get; set; } = 3;
    public int AnimationFrames { get; set; } = 1;
    public int AnimationFrameTicks { get; set; } = 5;
    public BunnyWorldLight DayLight { get; set; }
    public BunnyWorldLight NightLight { get; set; }
    private readonly SortedDictionary<int,string> frameDust = new();
    private static readonly UTF8Encoding Utf8 = new(false,true);
    private readonly List<string> maps = new();
    private int randomizedFrom;
    private float randomChance;
    private int[] choices=Array.Empty<int>();
    /// <summary>Troca a poeira conforme o wallFrameNumber real da célula.</summary>
    public void SetFrameDust(int frameNumber,string dustName) {
        if(frameNumber<0||frameNumber>2)throw new ArgumentOutOfRangeException(nameof(frameNumber));
        Id(dustName);frameDust[frameNumber]=dustName;
    }
    /// <summary>Na atualização aleatória dos frames, substitui o frame de origem com a probabilidade indicada.</summary>
    public void RandomizeFrame(int from,float chance,params int[] alternatives) {
        ArgumentNullException.ThrowIfNull(alternatives);
        if(from<0||from>2||!float.IsFinite(chance)||chance<0||chance>1||alternatives.Length<1||alternatives.Length>3||alternatives.Any(n=>n<0||n>2)||alternatives.Distinct().Count()!=alternatives.Length)
            throw new ArgumentException("Wall frame choice requires source/alternatives in 0..2 and chance in 0..1");
        randomizedFrom=from;randomChance=chance;choices=(int[])alternatives.Clone();
    }
    public void AddMapEntry(int r,int g,int b,string name) {
        if((uint)r>255||(uint)g>255||(uint)b>255)throw new ArgumentOutOfRangeException(nameof(r));
        if(string.IsNullOrWhiteSpace(name)||Utf8.GetByteCount(name)>160||name.Any(c=>c<32||c==127)||maps.Count>=16)
            throw new ArgumentException("Wall map entry requires text up to 160 UTF8 bytes; limit 16");
        maps.Add(string.Join(",",r,g,b,Convert.ToHexString(Utf8.GetBytes(name))));
    }
    private static void Id(string value) {
        if(string.IsNullOrEmpty(value)||value.Length>72||!char.IsAsciiLetter(value[0])||value.Any(c=>!char.IsAsciiLetterOrDigit(c)&&c!='_'))
            throw new ArgumentException("Expected native ID field name: "+value);
    }
    // The loader serializes actual results of SetStaticDefaults. There are no
    // virtual gameplay hooks here until their managed callback bridges exist.
    public string NativeDefinition {
        get {
            if(BlendWallName.Length>0)Id(BlendWallName);
            if(DustName.Length>0)Id(DustName);
            void Range(string fieldName,int value,int min,int max) {
                if(value<min||value>max)throw new InvalidOperationException("[CSharp][ERROR][WallDefaults] Class="+GetType().FullName+
                    " Stage=Serialize Field="+fieldName+" Expected="+min+".."+max+" Actual="+value+" Reason=PolicyOutOfRange Result=Rejected");
            }
            Range(nameof(FailedDustCount),FailedDustCount,0,100);Range(nameof(DustCount),DustCount,0,100);
            Range(nameof(AnimationFrames),AnimationFrames,1,256);Range(nameof(AnimationFrameTicks),AnimationFrameTicks,1,255);
            return string.Join("\t","WALL",GetType().Name,Housing?"1":"0",BlendWallName.Length==0?"-":BlendWallName,DustName.Length==0?"-":DustName,
                FailedDustCount.ToString(CultureInfo.InvariantCulture),DustCount.ToString(CultureInfo.InvariantCulture),AnimationFrames.ToString(CultureInfo.InvariantCulture),AnimationFrameTicks.ToString(CultureInfo.InvariantCulture),
                randomizedFrom.ToString(CultureInfo.InvariantCulture),randomChance.ToString("R",CultureInfo.InvariantCulture),choices.Length==0?"-":string.Join(",",choices),
                frameDust.Count==0?"-":string.Join(",",frameDust.Select(p=>p.Key+":"+p.Value)),DayLight.Wire,NightLight.Wire,maps.Count==0?"-":string.Join(";",maps));
        }
    }
    public abstract void SetStaticDefaults();
}
