using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Text.Json;
using BunnyLoader.Source;

namespace BunnyLoader.SourceCompiler;

internal static class BasicItemWriter
{
    internal static string Write(object item)
    {
        string name=item.GetType().Name;
        T Get<T>(string p) => (T)(item.GetType().GetProperty(p)?.GetValue(item)
            ?? throw new InvalidOperationException(name+": propriedade ausente "+p));
        string[] keys={"Width","Height","MaxStack","Value","Rare","Damage","Crit","UseTime",
            "UseAnimation","UseStyle","Pick","Axe","Hammer"};
        int[] min={0,0,1,0,-11,0,-100,0,0,0,0,0,0};
        int[] max={256,256,9999,100000000,11,1000,100,600,600,20,1000,200,1000};
        var parts=new List<string>{"ITEM",name};
        for(int i=0;i<keys.Length;i++) {
            int value=Get<int>(keys[i]);
            if(value<min[i]||value>max[i]||(keys[i]=="Rare"&&value>-11&&value< -1))throw new InvalidOperationException(name+": "+keys[i]+" fora do limite");
            parts.Add(value.ToString(CultureInfo.InvariantCulture));
        }
        int flags=(Get<bool>("Melee")?1:0)|(Get<bool>("NoMelee")?2:0)|(Get<bool>("AutoReuse")?4:0)|
            (Get<bool>("UseTurn")?8:0)|(Get<bool>("Consumable")?16:0)|(Get<bool>("Accessory")?32:0)|(Get<bool>("Vanity")?64:0);
        parts.Add(flags.ToString(CultureInfo.InvariantCulture));
        float knock=Get<float>("KnockBack");
        if(!float.IsFinite(knock)||knock<0||knock>50)throw new InvalidOperationException(name+": KnockBack invalido");
        parts.Add(knock.ToString("R",CultureInfo.InvariantCulture));
        string sound=Get<string>("UseSound");
        if(sound.Length>72||sound.Any(c=>!(char.IsAsciiLetterOrDigit(c)||c=='_')))
            throw new InvalidOperationException(name+": UseSound deve ser nome de SoundID");
        parts.Add(sound.Length==0?"-":sound);
        string tooltip=Get<string>("Tooltip");
        if(Encoding.UTF8.GetByteCount(tooltip)>512||tooltip.Any(c=>c=='\0'||c=='\x7f'))
            throw new InvalidOperationException(name+": Tooltip invalido/grande demais");
        parts.Add(tooltip.Length==0?"-":Convert.ToHexString(Encoding.UTF8.GetBytes(tooltip)));
        // Executa o AddRecipes real do mod. Excecoes nao viram um registro ficticio.
        item.GetType().GetMethod("AddRecipes",Type.EmptyTypes)!.Invoke(item,null);
        var recipes=Get<IReadOnlyList<string>>("RecipeDefinitions");
        parts.Add(recipes.Count==0?"-":string.Join(";",recipes));
        string behavior=Get<string>("BehaviorDefinition");
        if(behavior.Length!=0)behavior="{\"class\":\""+item.GetType().FullName+"\","+behavior.Substring(1);
        if(Encoding.UTF8.GetByteCount(behavior)>4096)throw new InvalidOperationException(name+": comportamento excede 4 KiB");
        string placement=Get<string>("PlacementDefinition");
        if(behavior.Length!=0||placement.Length!=0)parts.Add(behavior.Length==0?"-":Convert.ToHexString(Encoding.UTF8.GetBytes(behavior)));
        if(placement.Length!=0)parts.Add(placement);
        var localized=Get<Dictionary<string,string>>("TooltipTranslations");
        if(localized.Count>0) {
            if(localized.Count>8)throw new InvalidOperationException(name+": mais de 8 traducoes de tooltip");
            while(parts.Count<22)parts.Add("-");
            var values=new List<string>();
            foreach(var pair in localized.OrderBy(p=>p.Key,StringComparer.Ordinal)) {
                var culture=pair.Key;
                var text=pair.Value;
                if(culture.Length<2||culture.Length>20||
                    culture.Any(c=>!(char.IsAsciiLetterOrDigit(c)||c=='-'))||
                    string.IsNullOrWhiteSpace(text)||Encoding.UTF8.GetByteCount(text)>512||
                    text.Any(c=>c=='\0'||c=='\x7f'))
                    throw new InvalidOperationException(name+": traducao de tooltip invalida");
                values.Add(culture+":"+Convert.ToHexString(Encoding.UTF8.GetBytes(text)));
            }
            parts.Add(string.Join(";",values));
        }
        int defense=Get<int>("Defense");
        if(defense<0||defense>1000)throw new InvalidOperationException(name+": Defense fora do limite");
        if(Get<bool>("HasDefenseOverride")) {
            while(parts.Count<23)parts.Add("-");
            parts.Add(defense.ToString(CultureInfo.InvariantCulture));
        }
        string accessoryEffects=Get<string>("NativeAccessoryEffectsDefinition");
        if(accessoryEffects.Length!=0) {
            if(!Get<bool>("Accessory"))throw new InvalidOperationException(name+": politica nativa requer Accessory=true");
            while(parts.Count<24)parts.Add("-");
            parts.Add(accessoryEffects);
        }
        string armor=Get<string>("NativeArmorDefinition");
        if(armor.Length!=0) {
            if(Encoding.UTF8.GetByteCount(armor)>8192)throw new InvalidOperationException(name+": politica de armadura excede 8 KiB");
            while(parts.Count<25)parts.Add("-");
            parts.Add(armor);
        }
        var managed = (BunnyItem)item;
        var lines = new List<BunnyTooltipLine> {
            new("ItemName",Get<string>("DisplayName")),
            new("Tooltip0",tooltip)
        };
        managed.ModifyTooltips(lines);
        if(lines.Count>24)throw new InvalidOperationException(name+": tooltip line limit exceeded");
        var options = new List<object>();
        var names = new HashSet<string>(StringComparer.Ordinal);
        foreach(var line in lines) {
            if(!line.Visible)continue;
            if(string.IsNullOrEmpty(line.Name)||line.Name.Length>72||
                line.Name.Any(ch=>!char.IsAsciiLetterOrDigit(ch)&&ch!='_')||
                !names.Add(line.Name)||line.Text==null||Encoding.UTF8.GetByteCount(line.Text)>512)
                throw new InvalidOperationException(name+": invalid tooltip line");
            if(line.OverrideColor!=null&&
                (line.OverrideColor.Length!=6||line.OverrideColor.Any(ch=>!Uri.IsHexDigit(ch))))
                throw new InvalidOperationException(name+": invalid tooltip RGB");
            var draw = new BunnyDrawableTooltipLine(line.Name);
            bool defaultDraw = managed.PreDrawTooltipLine(draw);
            if(!float.IsFinite(draw.WaveAmplitude)||draw.WaveAmplitude<0||draw.WaveAmplitude>12||
                !float.IsFinite(draw.WaveSpeed)||draw.WaveSpeed<0||draw.WaveSpeed>32||
                !float.IsFinite(draw.LetterStep)||draw.LetterStep<0||draw.LetterStep>6||
                !float.IsFinite(draw.HueStep)||draw.HueStep<0||draw.HueStep>1||
                !float.IsFinite(draw.HueSpeed)||draw.HueSpeed<0||draw.HueSpeed>5)
                throw new InvalidOperationException(name+": invalid tooltip animation");
            options.Add(new {name=line.Name,text=line.Text,color=line.OverrideColor??"",
                draw=defaultDraw&&draw.DrawDefault, rainbow=draw.RainbowWave,
                amplitude=draw.WaveAmplitude,speed=draw.WaveSpeed,
                letterStep=draw.LetterStep,hueStep=draw.HueStep,hueSpeed=draw.HueSpeed});
        }
        bool active = options.Count!=2 ||
            lines[0].OverrideColor!=null||lines[1].OverrideColor!=null||
            managed.GetType().GetMethod(nameof(BunnyItem.ModifyTooltips))?.DeclaringType != typeof(BunnyItem)||
            managed.GetType().GetMethod(nameof(BunnyItem.PreDrawTooltipLine))?.DeclaringType != typeof(BunnyItem);
        if(active) {
            string plan = JsonSerializer.Serialize(options);
            if(Encoding.UTF8.GetByteCount(plan)>4096)
                throw new InvalidOperationException(name+": tooltip draw plan too large");
            while(parts.Count<26)parts.Add("-");
            parts.Add(Convert.ToHexString(Encoding.UTF8.GetBytes(plan)));
        }
        return string.Join("\t",parts);
    }
}
