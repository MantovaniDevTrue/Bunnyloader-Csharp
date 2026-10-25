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

Console.WriteLine("EXAMPLEMOD_BUFF_HOST_QA=PASS defense debuff minion locales");
