using System.Globalization;
using System.IO.Compression;
using System.Reflection;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using BunnyLoader.CSharp.Authoring;

namespace BunnyLoader.CSharp.Packager;

internal static partial class Program
{
    private static readonly DateTimeOffset FixedDate = new(2020, 1, 1, 0, 0, 0, TimeSpan.Zero);
    private static readonly UTF8Encoding Utf8 = new(false);

    private static int Main(string[] args)
    {
        if (args.Length != 4)
        {
            Console.Error.WriteLine("Usage: BunnyCsPackager <managed-mod.dll> <libmod.so> <Assets> <output.bmod>");
            return 2;
        }

        string stage = Path.Combine(Path.GetTempPath(), "bunny-csharp-pack-" + Guid.NewGuid().ToString("N"));
        try
        {
            string assemblyPath = Path.GetFullPath(args[0]);
            string libraryPath = Path.GetFullPath(args[1]);
            string assetsPath = Path.GetFullPath(args[2]);
            string outputPath = Path.GetFullPath(args[3]);

            if (!File.Exists(assemblyPath) || !File.Exists(libraryPath) || !Directory.Exists(assetsPath))
                throw new FileNotFoundException("The managed C# assembly, native library and asset folder must exist.");

            Assembly assembly = Assembly.LoadFrom(assemblyPath);
            Type[] modules = assembly.GetTypes()
                .Where(t => typeof(BunnyContentModule).IsAssignableFrom(t) && !t.IsAbstract)
                .ToArray();

            if (modules.Length != 1 || Activator.CreateInstance(modules[0]) is not BunnyContentModule module)
                throw new InvalidOperationException("The mod must expose exactly one public BunnyContentModule.");

            BunnyModInfo info = module.Info;
            BunnyContentRegistry registry = new();
            module.Register(registry);
            Validate(info, registry);

            Directory.CreateDirectory(stage);
            string contentRoot = Path.Combine(stage, "content");
            string generatedRoot = Path.Combine(contentRoot, "Content");
            Directory.CreateDirectory(generatedRoot);
            Write(Path.Combine(stage, "manifest.json"), JsonSerializer.Serialize(new
            {
                uid = info.Uid,
                id = info.Id,
                name = info.Name,
                version = info.Version,
                authors = new[] { new { name = info.Author, role = "NativeAOT C# mod" } },
                category = info.Category,
                summary = info.Description,
                license = info.License,
                blVersion = 2,
                gameVersion = new[] { info.TerrariaBuild, info.TerrariaBuild },
                entry = "main.js",
                dependencies = Array.Empty<string>()
            }, new JsonSerializerOptions { WriteIndented = true }));
            Write(Path.Combine(contentRoot, "main.js"),
                "export default class GeneratedBunnyCSharpMod extends Mod {\n" +
                "    Load() { bl.log(" + JsonSerializer.Serialize(info.Name + ": NativeAOT package loaded") + "); }\n" +
                "}\n");

            foreach (BunnyProjectile projectile in registry.Projectiles)
                Generate(generatedRoot, "Projectiles", projectile.Name, ProjectileJs(projectile));

            foreach (BunnyWeapon weapon in registry.Weapons)
                Generate(generatedRoot, "Items/Weapons", weapon.Name, WeaponJs(weapon));

            foreach (BunnyNpc npc in registry.Npcs)
                Generate(generatedRoot, "NPCs", npc.Name, NpcJs(npc, registry));

            foreach (BunnySummonItem summon in registry.SummonItems)
                Generate(generatedRoot, "Items/Consumables", summon.Name, SummonJs(summon));

            foreach (BunnyAccessory accessory in registry.Accessories)
                Generate(generatedRoot, "Items/Accessories", accessory.Name, AccessoryJs(accessory));

            foreach (BunnyArmor armor in registry.Armor)
                Generate(generatedRoot, "Items/Armor", armor.Name, ArmorJs(armor, registry));

            foreach (BunnyBuff buff in registry.Buffs)
                Generate(generatedRoot, "Buffs", buff.Name, BuffJs(buff));

            foreach (BunnyBuffPotion potion in registry.BuffPotions)
                Generate(generatedRoot, "Items/Consumables", potion.Name, BuffPotionJs(potion));

            foreach (BunnyMaterial material in registry.Materials)
                Generate(generatedRoot, "Items/Materials", material.Name, MaterialJs(material));

            string sourceAssets = Path.Combine(assetsPath, "Content");
            if (!Directory.Exists(sourceAssets))
                throw new DirectoryNotFoundException("Assets/Content folder is required.");

            foreach (string image in Directory.GetFiles(sourceAssets, "*.png", SearchOption.AllDirectories))
            {
                string relative = Path.GetRelativePath(sourceAssets, image);
                string destination = Path.Combine(generatedRoot, relative);
                Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
                File.Copy(image, destination, true);
            }

            foreach (string expected in registry.Projectiles.Select(p => "Projectiles/" + p.Name)
                         .Concat(registry.Weapons.Select(w => "Items/Weapons/" + w.Name))
                         .Concat(registry.Npcs.Select(n => "NPCs/" + n.Name))
                         .Concat(registry.SummonItems.Select(s => "Items/Consumables/" + s.Name))
                         .Concat(registry.Accessories.Select(a => "Items/Accessories/" + a.Name))
                         .Concat(registry.Armor.Select(a => "Items/Armor/" + a.Name))
                         .Concat(registry.Buffs.Select(b => "Buffs/" + b.Name))
                         .Concat(registry.BuffPotions.Select(b => "Items/Consumables/" + b.Name))
                         .Concat(registry.Materials.Select(m => "Items/Materials/" + m.Name)))
            {
                if (!File.Exists(Path.Combine(generatedRoot, expected.Replace('/', Path.DirectorySeparatorChar) + ".png")))
                    throw new FileNotFoundException("Required sprite is missing: " + expected + ".png");
            }

            string nativeDest = Path.Combine(stage, "native", "arm64-v8a", "libmod.so");
            Directory.CreateDirectory(Path.GetDirectoryName(nativeDest)!);
            File.Copy(libraryPath, nativeDest, true);
            Directory.CreateDirectory(Path.GetDirectoryName(outputPath)!);

            using (FileStream output = File.Create(outputPath))
            using (ZipArchive zip = new(output, ZipArchiveMode.Create))
            {
                foreach (string source in Directory.GetFiles(stage, "*", SearchOption.AllDirectories)
                             .OrderBy(s => Path.GetRelativePath(stage, s), StringComparer.Ordinal))
                {
                    string name = Path.GetRelativePath(stage, source).Replace('\\', '/');
                    ZipArchiveEntry entry = zip.CreateEntry(name, CompressionLevel.Optimal);
                    entry.LastWriteTime = FixedDate;
                    using Stream input = File.OpenRead(source);
                    using Stream target = entry.Open();
                    input.CopyTo(target);
                }
            }

            Console.WriteLine("BUNNY_CSHARP_AUTHORING_OK=" + outputPath);
            Console.WriteLine("BUNNY_GENERATED_PROJECTILES=" + registry.Projectiles.Count);
            Console.WriteLine("BUNNY_GENERATED_WEAPONS=" + registry.Weapons.Count);
            Console.WriteLine("BUNNY_GENERATED_NPCS=" + registry.Npcs.Count);
            Console.WriteLine("BUNNY_GENERATED_SUMMONS=" + registry.SummonItems.Count);
            Console.WriteLine("BUNNY_GENERATED_ACCESSORIES=" + registry.Accessories.Count);
            Console.WriteLine("BUNNY_GENERATED_ARMOR=" + registry.Armor.Count);
            Console.WriteLine("BUNNY_GENERATED_ARMOR_SETS=" + registry.ArmorSets.Count);
            Console.WriteLine("BUNNY_GENERATED_BUFFS=" + registry.Buffs.Count);
            Console.WriteLine("BUNNY_GENERATED_POTIONS=" + registry.BuffPotions.Count);
            Console.WriteLine("BUNNY_GENERATED_MATERIALS=" + registry.Materials.Count);
            Console.WriteLine("BUNNY_GENERATED_NPC_DROPS=" + registry.NpcDrops.Count);
            return 0;
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine(exception.ToString());
            return 1;
        }
        finally
        {
            if (Directory.Exists(stage)) Directory.Delete(stage, true);
        }
    }

    private static void Validate(BunnyModInfo info, BunnyContentRegistry content)
    {
        Id(info.Id);
        if (!Guid.TryParse(info.Uid, out _) || string.IsNullOrWhiteSpace(info.Version))
            throw new InvalidOperationException("Invalid module UID or version.");
        HashSet<string> types = new(StringComparer.Ordinal);
        foreach (string name in content.Projectiles.Select(p => p.Name)
                     .Concat(content.Weapons.Select(w => w.Name))
                     .Concat(content.Npcs.Select(n => n.Name))
                     .Concat(content.SummonItems.Select(s => s.Name))
                     .Concat(content.Accessories.Select(a => a.Name))
                     .Concat(content.Armor.Select(a => a.Name))
                     .Concat(content.Buffs.Select(b => b.Name))
                     .Concat(content.BuffPotions.Select(b => b.Name))
                     .Concat(content.Materials.Select(m => m.Name)))
        {
            Id(name);
            if (!types.Add(name)) throw new InvalidOperationException("Duplicate content name: " + name);
        }
        HashSet<string> projectiles = content.Projectiles.Select(p => p.Name).ToHashSet(StringComparer.Ordinal);
        HashSet<string> npcs = content.Npcs.Select(n => n.Name).ToHashSet(StringComparer.Ordinal);
        foreach (BunnyWeapon item in content.Weapons)
        {
            if (!projectiles.Contains(item.Projectile)) throw new InvalidOperationException("Unknown projectile: " + item.Projectile);
            Id(item.Rarity);
            Id(item.UseSound);
            if (item.Damage <= 0 || item.WoodCost < 0 || item.ShootSpeed <= 0 || item.UseTime <= 0)
                throw new InvalidOperationException("Invalid weapon settings: " + item.Name);
        }
        foreach (BunnySummonItem item in content.SummonItems)
        {
            if (!npcs.Contains(item.Npc)) throw new InvalidOperationException("Unknown NPC: " + item.Npc);
            Id(item.Rarity);
        }
        ValidateAdvanced(content);
        foreach (BunnyNpc npc in content.Npcs)
        {
            Id(npc.AnimationNpc);
            Id(npc.HitSound);
            Id(npc.DeathSound);
            if (npc.CombatProjectile is not null && !projectiles.Contains(npc.CombatProjectile))
                throw new InvalidOperationException("Unknown combat projectile: " + npc.CombatProjectile);
        }
    }

    private static void Id(string value)
    {
        if (!Regex.IsMatch(value, @"^[A-Za-z][A-Za-z0-9_]*$", RegexOptions.CultureInvariant))
            throw new InvalidOperationException("Invalid content identifier: " + value);
    }

    private static string Number(float value)
    {
        if (!float.IsFinite(value)) throw new InvalidOperationException("Non-finite content value.");
        return value.ToString("0.########", CultureInfo.InvariantCulture);
    }

    private static void Write(string path, string text)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, text, Utf8);
    }

    private static void Generate(string root, string folder, string name, string text)
        => Write(Path.Combine(root, folder.Replace('/', Path.DirectorySeparatorChar), name + ".js"), text);

    private static string ProjectileJs(BunnyProjectile p) => """
        export class @NAME@ extends ModProjectile {
            SetDefaults() {
                this.Projectile.width = @WIDTH@;
                this.Projectile.height = @HEIGHT@;
                this.Projectile.aiStyle = @STYLE@;
                this.Projectile.friendly = @FRIENDLY@;
                this.Projectile.hostile = @HOSTILE@;
                this.Projectile.ranged = @RANGED@;
                this.Projectile.penetrate = @PENETRATE@;
                this.Projectile.timeLeft = @TIME@;
                this.Projectile.ignoreWater = @IGNORE@;
                this.Projectile.tileCollide = @COLLIDE@;
            }
        }
        """.Replace("@NAME@", p.Name).Replace("@WIDTH@", p.Width.ToString(CultureInfo.InvariantCulture))
        .Replace("@HEIGHT@", p.Height.ToString(CultureInfo.InvariantCulture))
        .Replace("@STYLE@", p.AiStyle.ToString(CultureInfo.InvariantCulture))
        .Replace("@FRIENDLY@", p.Friendly ? "true" : "false")
        .Replace("@HOSTILE@", p.Hostile ? "true" : "false")
        .Replace("@RANGED@", p.Ranged ? "true" : "false")
        .Replace("@PENETRATE@", p.Penetrate.ToString(CultureInfo.InvariantCulture))
        .Replace("@TIME@", p.TimeLeft.ToString(CultureInfo.InvariantCulture))
        .Replace("@IGNORE@", p.IgnoreWater ? "true" : "false")
        .Replace("@COLLIDE@", p.TileCollide ? "true" : "false");

    private static string WeaponJs(BunnyWeapon w) => """
        const { ItemID, TileID, SoundID } = Terraria.ID;

        export class @NAME@ extends ModItem {
            SetDefaults() {
                this.Item.ranged = true;
                this.Item.shoot = ModProjectile.getTypeByName('@PROJECTILE@');
                this.Item.shootSpeed = @SPEED@;
                this.SetWeaponValues(@DAMAGE@, @KNOCKBACK@, @CRIT@);
                this.SetDefaultWeaponStyle(@USETIME@, true);
                this.Item.noMelee = true;
                this.Item.rare = ItemRarityID.@RARITY@;
                this.Item.UseSound = SoundID.@SOUND@;
            }

            AddRecipes() {
                this.CreateRecipe()
                    .AddIngredient(ItemID.Wood, @WOOD@)
                    .AddTile(TileID.WorkBenches)
                    .Register();
            }
        }
        """.Replace("@NAME@", w.Name).Replace("@PROJECTILE@", w.Projectile)
        .Replace("@SPEED@", Number(w.ShootSpeed))
        .Replace("@DAMAGE@", w.Damage.ToString(CultureInfo.InvariantCulture))
        .Replace("@KNOCKBACK@", w.Knockback.ToString(CultureInfo.InvariantCulture))
        .Replace("@CRIT@", w.Crit.ToString(CultureInfo.InvariantCulture))
        .Replace("@USETIME@", w.UseTime.ToString(CultureInfo.InvariantCulture))
        .Replace("@RARITY@", w.Rarity).Replace("@SOUND@", w.UseSound)
        .Replace("@WOOD@", w.WoodCost.ToString(CultureInfo.InvariantCulture));

    private static string NpcJs(BunnyNpc n, BunnyContentRegistry registry)
    {
        string combat = n.CombatProjectile is null ? "" : """
                AI(npc) {
                    if (Terraria.Main.netMode !== 0) return;
                    const attack = bl.csharp.npcAttack(npc.whoAmI, npc.type);
                    if (!attack) return;
                    const shotType = ModProjectile.getTypeByName('@SHOT@');
                    if (shotType < 0) return;
                    const player = Terraria.Main.player[Terraria.Main.myPlayer];
                    const source = player.GetProjectileSource_Item(player.HeldItem);
                    const spawn = Vector2.new(attack.x, attack.y);
                    const owner = player.whoAmI;
                    NewProjectile(source, spawn, Vector2.new(attack.vx0, attack.vy0), shotType,
                                  attack.damage, 0, owner, 0, 0, 0, null);
                    if (attack.count === 2) {
                        NewProjectile(source, spawn, Vector2.new(attack.vx1, attack.vy1), shotType,
                                      attack.damage, 0, owner, 0, 0, 0, null);
                    }
                }

            """.Replace("@SHOT@", n.CombatProjectile);

        string declaration = n.CombatProjectile is null ? "" :
            "const NewProjectile = Terraria.Projectile['int NewProjectile(IEntitySource spawnSource, Vector2 position, Vector2 velocity, int Type, int Damage, float KnockBack, int Owner, float ai0, float ai1, float ai2, NewProjectileModifier modifer)'];\n\n";

        return ("""
            const { NPCID, SoundID } = Terraria.ID;
            
            @DECLARATION@
            export class @NAME@ extends ModNPC {
                SetStaticDefaults() {
                    Terraria.Main.npcFrameCount[this.Type] = @FRAMECOUNT@;
                }

            @COMBAT@
            @DROPS@
                SetDefaults() {
                    this.NPC.width = @WIDTH@;
                    this.NPC.height = @HEIGHT@;
                    this.NPC.aiStyle = 0;
                    this.NPC.damage = @DAMAGE@;
                    this.NPC.defense = @DEFENSE@;
                    this.NPC.lifeMax = @LIFE@;
                    this.NPC.knockBackResist = @KNOCKBACK@;
                    this.NPC.HitSound = SoundID.@HITSOUND@;
                    this.NPC.DeathSound = SoundID.@DEATHSOUND@;
                    this.NPC.value = ModNPC.NPCValue(0, 0, 0, 30);
                    this.AnimationType = NPCID.@ANIMATION@;
                }
            }
            """).Replace("@DECLARATION@", declaration).Replace("@NAME@", n.Name)
            .Replace("@COMBAT@", combat).Replace("@DROPS@", NpcDropsJs(n, registry))
            .Replace("@FRAMECOUNT@", n.FrameCount.ToString(CultureInfo.InvariantCulture))
            .Replace("@WIDTH@", n.Width.ToString(CultureInfo.InvariantCulture))
            .Replace("@HEIGHT@", n.Height.ToString(CultureInfo.InvariantCulture))
            .Replace("@DAMAGE@", n.Damage.ToString(CultureInfo.InvariantCulture))
            .Replace("@DEFENSE@", n.Defense.ToString(CultureInfo.InvariantCulture))
            .Replace("@LIFE@", n.LifeMax.ToString(CultureInfo.InvariantCulture))
            .Replace("@KNOCKBACK@", Number(n.KnockBackResist))
            .Replace("@HITSOUND@", n.HitSound).Replace("@DEATHSOUND@", n.DeathSound)
            .Replace("@ANIMATION@", n.AnimationNpc);
    }

    private static string SummonJs(BunnySummonItem s) => """
        const { ItemID, TileID, ItemUseStyleID } = Terraria.ID;

        export class @NAME@ extends ModItem {
            SetDefaults() {
                this.Item.width = 30;
                this.Item.height = 20;
                this.Item.maxStack = 1;
                this.Item.useTime = 25;
                this.Item.useAnimation = 25;
                this.Item.useStyle = ItemUseStyleID.HoldUp;
                this.Item.rare = ItemRarityID.@RARITY@;
                this.Item.consumable = false;
            }

            CanUseItem(item, player) {
                return Terraria.Main.netMode === 0 &&
                    !Terraria.NPC.AnyNPCs(ModNPC.getTypeByName('@NPC@'));
            }

            UseItem(item, player) {
                if (player.whoAmI !== Terraria.Main.myPlayer) return true;
                const type = ModNPC.getTypeByName('@NPC@');
                Terraria.NPC.SpawnOnPlayer(player.whoAmI, type, 0, 0, 0, 0);
                return true;
            }

            AddRecipes() {
                this.CreateRecipe()
                    .AddIngredient(ItemID.Wood, @WOOD@)
                    .AddTile(TileID.WorkBenches)
                    .Register();
            }
        }
        """.Replace("@NAME@", s.Name).Replace("@NPC@", s.Npc).Replace("@RARITY@", s.Rarity)
        .Replace("@WOOD@", s.WoodCost.ToString(CultureInfo.InvariantCulture));
}
