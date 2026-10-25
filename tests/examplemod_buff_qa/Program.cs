using System.Globalization;
using System.Text.Json;
using BunnyLoader.Source;
using ExampleModCSharp;

static void Check(bool ok, string reason)
{
    if (!ok) throw new InvalidOperationException(reason);
}

string root = "samples/ExampleModCSharp/Playable/content/Localization/";
using var english = JsonDocument.Parse(File.ReadAllText(root + "en-US.json"));
using var portuguese = JsonDocument.Parse(File.ReadAllText(root + "pt-BR.json"));

static string Localized(JsonDocument locale, string name) =>
    locale.RootElement.GetProperty("BuffDescription").GetProperty(name).GetString()!;

var defense = new ExampleDefenseBuff();
defense.SetDefaults();
Check(defense.DefenseBonus == 10 && defense.DefenseMultiplierPercent == 100 &&
      !defense.IsDebuff, "Defense buff adds precisely 10 without weakening armor");
Check(defense.Description == Localized(english, nameof(ExampleDefenseBuff)), "Defense description equals Bunny English locale");
Check(defense.Description.Replace("{0}", defense.DefenseBonus.ToString(CultureInfo.InvariantCulture)) ==
      "Increases the player's defense by 10", "Native compiler description interpolation");
Check(Localized(portuguese, nameof(ExampleDefenseBuff)).Replace("{0}", "10") ==
      "Aumenta a defesa do jogador em 10", "Original Portuguese defense localization");

var debuff = new ExampleDefenseDebuff();
debuff.SetDefaults();
Check(debuff.IsDebuff && debuff.DefenseMultiplierPercent == 75 &&
      debuff.DefenseBonus == 0, "Defense debuff reduces total defense by 25 percent");
Check(debuff.Description == Localized(english, nameof(ExampleDefenseDebuff)).Replace("{0}", "25"),
      "Defense debuff displays percentage rather than zero bonus");
Check(Localized(portuguese, nameof(ExampleDefenseDebuff)).Replace("{0}", "25") ==
      "Reduz a defesa do jogador em 25%", "Original Portuguese debuff localization");

var minion = new ExampleMinionBuff();
minion.SetDefaults();
Check(minion.NoSave && minion.NoTimeDisplay && !minion.IsDebuff &&
      minion.MinionProjectile == nameof(ExampleMinion),
      "Minion buff flags and exact native projectile identity");
Check(minion.Description == Localized(english, nameof(ExampleMinionBuff)),
      "Minion description equals original English locale");
var projectile = new ExampleMinion();
projectile.SetDefaults();
Check(projectile.Minion && projectile.TimeLeft == 18000 &&
      projectile.MinionSlots == 1f, "Live minion lifecycle matches buff refresh policy");

var blocky = new Blocky();
blocky.SetDefaults();
Check(blocky.IsDebuff && blocky.NoSave && blocky.NoTimeDisplay &&
      blocky.NurseCannotRemove && blocky.DefenseBonus == 0 &&
      blocky.DefenseMultiplierPercent == 100,
      "Blocky faithfully registers debuff, no-save, hidden timer and nurse immunity");
Check(blocky.Description == Localized(english, nameof(Blocky)) &&
      Localized(portuguese, nameof(Blocky)) == "Pulo mais alto",
      "Blocky uses original localized descriptions");

var mountBuff = new ExampleMountBuff();
mountBuff.SetDefaults();
Check(!mountBuff.IsDebuff && mountBuff.NoSave && mountBuff.NoTimeDisplay &&
      !mountBuff.NurseCannotRemove && mountBuff.DefenseBonus == 0,
      "Original mount buff keeps only no-save and no-time flags");
Check(mountBuff.Description == Localized(english, nameof(ExampleMountBuff)) &&
      Localized(portuguese, nameof(ExampleMountBuff)) == "Bancos de couro, 4 porta-copos",
      "Mount buff uses original translated text");

const string content = "samples/ExampleModCSharp/Playable/content/";
static string OriginalPngSha256(string path) =>
    Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(
        File.ReadAllBytes(content + path))).ToLowerInvariant();
Check(blocky.Texture == "Content/Buffs/Blocky.png" &&
      OriginalPngSha256(blocky.Texture) == "37433dc43b6657e57e9b0ff7cf179b5fa3fac67d3c901ae7a5b2a5498793cea0",
      "Blocky PNG is byte-for-byte the original uploaded ExampleModBunny image");
Check(mountBuff.Texture == "Content/Buffs/ExampleMountBuff.png" &&
      OriginalPngSha256(mountBuff.Texture) == "c76e9f47edf271782b09005fd0b9aa88621e354f9067bd980d96d27660189dfd",
      "Mount buff PNG is byte-for-byte original");

Console.WriteLine("EXAMPLEMOD_BUFF_HOST_QA=PASS defense debuff minion blocky mount locale and sprites (host only)");
