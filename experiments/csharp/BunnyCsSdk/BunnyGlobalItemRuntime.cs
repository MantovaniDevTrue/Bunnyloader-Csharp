using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;

namespace BunnyLoader.Source;

/// <summary>
/// Avalia os <see cref="BunnyGlobalItem"/> de um mod sobre a foto dos itens
/// vanilla escrita pelo host e devolve as políticas nativas (GLOBAL_ITEMS_V1).
/// Roda uma vez por mod no MonoVM do jogo, fora do loop de frames.
/// </summary>
public static class BunnyGlobalItemRuntime
{
    public const string SnapshotHeader = "BUNNY_ITEM_SNAPSHOT_V1";
    public const string WireHeader = "GLOBAL_ITEMS_V1";
    private const int MaxItems = 16384;
    private const int MaxWireBytes = 1 << 20;

    /// <summary>Tipos concretos de BunnyGlobalItem do assembly que sobrescrevem hooks de conteúdo.</summary>
    public static Type[] ContentGlobals(Assembly assembly) =>
        assembly.GetTypes()
            .Where(type => type.IsClass && !type.IsAbstract && !type.ContainsGenericParameters &&
                           typeof(BunnyGlobalItem).IsAssignableFrom(type) && ContentMask(type) != 0)
            .OrderBy(type => type.FullName, StringComparer.Ordinal)
            .ToArray();

    /// <summary>
    /// Bits dos hooks de conteúdo declarados no próprio tipo:
    /// 1 AppliesToEntity, 2 SetDefaults, 4 ModifyTooltips, 8 ModifyWeaponDamage,
    /// 16 ModifyWeaponCrit, 32 ModifyWeaponKnockback, 64 UpdateEquip,
    /// 128 CanShoot, 256 ModifyShootStats, 512 Shoot.
    /// </summary>
    public static int ContentMask(Type type)
    {
        var item = typeof(BunnyVanillaItem);
        var hooks = new (string Name, Type[] Parameters)[]
        {
            (nameof(BunnyGlobalItem.AppliesToEntity), new[] { item }),
            (nameof(BunnyGlobalItem.SetDefaults), new[] { item }),
            (nameof(BunnyGlobalItem.ModifyTooltips), new[] { item, typeof(List<BunnyTooltipLine>) }),
            (nameof(BunnyGlobalItem.ModifyWeaponDamage), new[] { item, typeof(StatModifier).MakeByRefType() }),
            (nameof(BunnyGlobalItem.ModifyWeaponCrit), new[] { item, typeof(float).MakeByRefType() }),
            (nameof(BunnyGlobalItem.ModifyWeaponKnockback), new[] { item, typeof(StatModifier).MakeByRefType() }),
            (nameof(BunnyGlobalItem.UpdateEquip), new[] { item, typeof(BunnyEquipEffects) }),
            (nameof(BunnyGlobalItem.CanShoot), new[] { typeof(BunnyShootContext) }),
            (nameof(BunnyGlobalItem.ModifyShootStats), new[] { typeof(BunnyShootContext), typeof(BunnyShot).MakeByRefType() }),
            (nameof(BunnyGlobalItem.Shoot), new[] { typeof(BunnyShootContext), typeof(BunnyShot) }),
        };
        int mask = 0;
        for (int i = 0; i < hooks.Length; i++)
        {
            MethodInfo? method = type.GetMethod(hooks[i].Name, BindingFlags.Public | BindingFlags.Instance,
                null, hooks[i].Parameters, null);
            if (method != null && method.DeclaringType != typeof(BunnyGlobalItem)) mask |= 1 << i;
        }
        // Só AppliesToEntity não produz nada.
        return mask == 1 ? 0 : mask;
    }

    /// <summary>Máscara do NativeShooting (CanShoot=1, ModifyShootStats=2, Shoot=4).</summary>
    public static int ShootMask(Type type) => (ContentMask(type) >> 7) & 7;

    /// <summary>Prefixo do dono enviado ao BunnyShootRuntime para GlobalItems.</summary>
    public const string ShootOwnerPrefix = "@GlobalItem:";

    internal static List<(int Type, double[] Values)> ReadSnapshot(string text)
    {
        var lines = text.Split('\n');
        if (lines.Length < 1) throw new FormatException("Snapshot vazio");
        var header = lines[0].TrimEnd('\r').Split('\t');
        if (header.Length < 2 || header[0] != SnapshotHeader || header[1] != "type")
            throw new FormatException("Cabeçalho de snapshot inválido");
        // Coluna do snapshot -> índice do campo; campos ausentes ficam 0.
        var map = new int[header.Length];
        for (int c = 2; c < header.Length; c++)
            map[c] = BunnyVanillaItem.FieldIndex.TryGetValue(header[c], out int index) ? index : -1;
        var rows = new List<(int, double[])>();
        var seen = new HashSet<int>();
        for (int l = 1; l < lines.Length; l++)
        {
            string line = lines[l].TrimEnd('\r');
            if (line.Length == 0) continue;
            var cells = line.Split('\t');
            if (cells.Length != header.Length) throw new FormatException("Linha de snapshot com colunas erradas: " + l);
            if (!int.TryParse(cells[1], NumberStyles.Integer, CultureInfo.InvariantCulture, out int type) ||
                type < 1 || type >= MaxItems || !seen.Add(type))
                throw new FormatException("Tipo de item inválido no snapshot: " + cells[1]);
            var values = new double[BunnyVanillaItem.Fields.Length];
            for (int c = 2; c < cells.Length; c++)
            {
                if (map[c] < 0) continue;
                if (!double.TryParse(cells[c], NumberStyles.Float, CultureInfo.InvariantCulture, out double value) ||
                    double.IsNaN(value) || double.IsInfinity(value))
                    throw new FormatException("Valor inválido no snapshot: " + header[c] + "=" + cells[c]);
                values[map[c]] = value;
            }
            rows.Add((type, values));
            if (rows.Count > MaxItems) throw new FormatException("Snapshot grande demais");
        }
        return rows;
    }

    private static string F(float value) => value.ToString("R", CultureInfo.InvariantCulture);
    private static string F(double value, BunnyVanillaItem.Kind kind) => kind switch
    {
        BunnyVanillaItem.Kind.Bool => value != 0 ? "b:1" : "b:0",
        BunnyVanillaItem.Kind.Float => "f:" + ((float)value).ToString("R", CultureInfo.InvariantCulture),
        _ => "i:" + ((int)value).ToString(CultureInfo.InvariantCulture),
    };

    /// <summary>Chave de classe do wire (V0..V9 vanilla ou C:Namespace.Tipo do mesmo assembly).</summary>
    internal static string ClassKey(DamageClass damageClass, Assembly owner)
    {
        for (int i = 0; i < 10; ++i)
            if (ReferenceEquals(damageClass, DamageClassLoader.GetDamageClass(i)))
                return "V" + i.ToString(CultureInfo.InvariantCulture);
        Type custom = damageClass.GetType();
        if (custom.Assembly != owner || custom.FullName is null || custom.FullName.Length > 240)
            throw new InvalidOperationException("ClassBonus requer classe vanilla ou do próprio mod");
        return "C:" + custom.FullName;
    }

    /// <summary>Avalia os GlobalItems de <paramref name="assembly"/> sobre o snapshot.</summary>
    public static string Evaluate(Assembly assembly, string snapshotText)
    {
        var globals = ContentGlobals(assembly)
            .Select(type => (BunnyGlobalItem)(Activator.CreateInstance(type)
                ?? throw new InvalidOperationException("GlobalItem sem construtor: " + type.FullName)))
            .ToArray();
        var output = new StringBuilder(WireHeader).Append('\n');
        if (globals.Length == 0) return output.ToString();
        foreach (var (type, values) in ReadSnapshot(snapshotText))
        {
            var item = new BunnyVanillaItem(type, values);
            var applying = globals.Where(global => global.AppliesToEntity(item)).ToArray();
            if (applying.Length == 0) continue;

            item.Writable = true;
            foreach (var global in applying) global.SetDefaults(item);
            item.Writable = false;
            var changes = item.Changes().Select(change => change.Field.Native + ":" + F(change.Value, change.Field.Type)).ToArray();
            if (changes.Length > 0)
                output.Append("DEFAULTS\t").Append(type).Append('\t').Append(string.Join(";", changes)).Append('\n');

            var tooltips = new List<BunnyTooltipLine>();
            foreach (var global in applying) global.ModifyTooltips(item, tooltips);
            var visible = tooltips.Where(line => line.Visible && !string.IsNullOrEmpty(line.Text)).ToArray();
            if (visible.Length > 8) throw new InvalidOperationException("ModifyTooltips: no máximo 8 linhas por item vanilla");
            if (visible.Length > 0)
            {
                var hex = visible.Select(line =>
                {
                    byte[] bytes = Encoding.UTF8.GetBytes(line.Text);
                    if (bytes.Length > 120 || line.Text.Any(char.IsControl))
                        throw new InvalidOperationException("ModifyTooltips: linha inválida ou maior que 120 bytes");
                    return Convert.ToHexString(bytes);
                });
                output.Append("TOOLTIP\t").Append(type).Append('\t').Append(string.Join(",", hex)).Append('\n');
            }

            StatModifier damage = StatModifier.Default, knockback = StatModifier.Default;
            float crit = 0f;
            foreach (var global in applying)
            {
                global.ModifyWeaponDamage(item, ref damage);
                global.ModifyWeaponCrit(item, ref crit);
                global.ModifyWeaponKnockback(item, ref knockback);
            }
            if (damage != StatModifier.Default)
                output.Append("DAMAGE\t").Append(type).Append('\t').Append(F(damage.Base)).Append('\t')
                    .Append(F(damage.Additive)).Append('\t').Append(F(damage.Multiplicative)).Append('\t')
                    .Append(F(damage.Flat)).Append('\n');
            if (crit != 0f)
            {
                if (!float.IsFinite(crit)) throw new InvalidOperationException("ModifyWeaponCrit: valor não finito");
                output.Append("CRIT\t").Append(type).Append('\t').Append(F(crit)).Append('\n');
            }
            if (knockback != StatModifier.Default)
                output.Append("KNOCKBACK\t").Append(type).Append('\t').Append(F(knockback.Base)).Append('\t')
                    .Append(F(knockback.Additive)).Append('\t').Append(F(knockback.Multiplicative)).Append('\t')
                    .Append(F(knockback.Flat)).Append('\n');

            var effects = new BunnyEquipEffects();
            foreach (var global in applying) global.UpdateEquip(item, effects);
            // Disparo: só itens que atiram; o dono lista as classes (ordem alfabética, como a avaliação).
            var shooters = item.Shoot > 0 ? applying.Where(global => ShootMask(global.GetType()) != 0).ToArray() : Array.Empty<BunnyGlobalItem>();
            if (shooters.Length > 0)
            {
                int shootMask = shooters.Aggregate(0, (mask, global) => mask | ShootMask(global.GetType()));
                string owner = ShootOwnerPrefix + string.Join(",", shooters.Select(global => global.GetType().FullName));
                if (owner.Length > 1024) throw new InvalidOperationException("Shoot: GlobalItems demais para o mesmo item");
                output.Append("SHOOT\t").Append(type).Append('\t').Append(shootMask).Append('\t').Append(owner).Append('\n');
            }
            foreach (var bonus in effects.ClassBonuses)
                output.Append(string.Join("\t", "CLASS_BONUS", type.ToString(CultureInfo.InvariantCulture),
                    ClassKey(bonus.Class, assembly),
                    "0", F(1f + bonus.Damage), F(bonus.DamageMultiplier), "0",
                    "0", F(1f + bonus.Knockback), F(bonus.KnockbackMultiplier), "0",
                    F(bonus.Crit), F(bonus.AttackSpeed), F(bonus.ArmorPenetration))).Append('\n');
            if (output.Length > MaxWireBytes) throw new InvalidOperationException("Políticas de GlobalItem grandes demais");
        }
        return output.ToString();
    }

    /// <summary>Lê o snapshot do arquivo do host e avalia; erro vira linha ERROR.</summary>
    public static string EvaluateFile(Assembly assembly, string snapshotPath)
    {
        try
        {
            var info = new FileInfo(snapshotPath);
            if (!info.Exists || info.Length > 8 << 20) throw new IOException("Snapshot ausente ou grande demais");
            return Evaluate(assembly, File.ReadAllText(snapshotPath, Encoding.UTF8));
        }
        catch (Exception error)
        {
            return "ERROR\t" + error.GetType().Name + ": " + error.Message.Replace('\n', ' ').Replace('\t', ' ');
        }
    }
}
