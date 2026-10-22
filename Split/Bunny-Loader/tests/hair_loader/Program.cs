using BunnyLoader.Source;

Check.Run();

static class Check
{
    private static int passed;

    private static void True(bool result, string name)
    {
        if (!result) throw new InvalidOperationException("Falha: " + name);
        passed++;
        Console.WriteLine("OK " + name);
    }

    private static void Throws<TException>(Action action, string name)
        where TException : Exception
    {
        try
        {
            action();
        }
        catch (TException)
        {
            True(true, name);
            return;
        }
        throw new InvalidOperationException("Não lançou " + typeof(TException).Name + ": " + name);
    }

    public static void Run()
    {
        var assets = new HashSet<string>(StringComparer.Ordinal)
        {
            "Content/Hairs/ProgressHair.png",
            "Content/Hairs/AltHair.png",
            "Content/Hairs/AltHair_Alt.png"
        };
        bool Exists(string name) => assets.Contains(name);

        var loader = new HairLoader();
        var progress = new ProgressHair();
        HairDefinition registered = loader.Register("mod-a", progress, Exists);

        True(registered.Type == HairLoader.VanillaHairCount, "primeiro tipo depois do vanilla");
        True(registered.FullName == "mod-a/ProgressHair", "identidade estável UID/classe");
        True(progress.StaticDefaultsCalls == 1, "SetStaticDefaults exatamente uma vez");
        True(progress.AutoDefaultsCalls == 1, "AutoStaticDefaults exatamente uma vez");
        True(registered.MainTexture == registered.AltTexture, "textura alternativa ausente usa a principal");
        True(progress.Type == registered.Type, "ID chega à instância C#");
        True(progress.TypeSeenAtStaticDefaults == registered.Type &&
             progress.TypeSeenAtAutoDefaults == registered.Type,
             "Type existe antes dos defaults, como no tModLoader");
        True(loader.TryGet(registered.Type, out ModHair? loaded) &&
             ReferenceEquals(progress, loaded), "lookup por tipo");
        True(loader.TryResolveSavedHair("mod-a/ProgressHair", out int restored) &&
             restored == registered.Type, "carregamento pelo nome salvo");

        True(loader.GetUnlockedTypes(true, false).Contains(progress.Type),
            "penteado liberado na criação");
        True(!loader.GetUnlockedTypes(false, true).Contains(progress.Type),
            "condição do Cabeleireiro inicialmente fechada");
        progress.BossDowned = true;
        True(loader.GetUnlockedTypes(false, true).Contains(progress.Type),
            "condição de progressão reavaliada sem cache permanente");
        True(loader.GetUnlockedTypes(false, false).Count == 0,
            "contexto externo não libera penteado");

        var alternate = new AltHair();
        HairDefinition second = loader.Register("mod-b", alternate, Exists);
        True(second.Type == registered.Type + 1, "tipos sequenciais determinísticos");
        True(second.AltTexture == "Content/Hairs/AltHair_Alt",
            "textura com chapéu permanece distinta");
        True(second.RandomGender == Gender.Female,
            "gênero aleatório compatível");
        True(!second.CreationAvailable && registered.CreationAvailable,
            "disponibilidade na criacao fica no descritor");
        string expectedManifest =
            "BUNNY_HAIR_MANIFEST_V1\n" +
            "HAIR\t228\tmod-a\tProgressHair\tContent/Hairs/ProgressHair.png\tContent/Hairs/ProgressHair.png\t0\t1\n" +
            "HAIR\t229\tmod-b\tAltHair\tContent/Hairs/AltHair.png\tContent/Hairs/AltHair_Alt.png\t2\t0\n";
        True(loader.ExportNativeManifest() == expectedManifest,
            "manifesto Mono->IL2CPP deterministico e versionado");
        True(!loader.GetUnlockedTypes(true, false).Contains(second.Type),
            "restrição de criação não altera Stylist");
        True(loader.GetUnlockedTypes(false, true).Contains(second.Type),
            "condições vazias liberam Cabeleireiro");

        Throws<InvalidOperationException>(() =>
            loader.Register("mod-a", new ProgressHair(), Exists), "UID/classe duplicados");
        Throws<InvalidOperationException>(() =>
            loader.Register("mod-b", alternate, Exists), "instância já registrada");
        Throws<InvalidOperationException>(() =>
            loader.Register("mod-c", new NoTextureHair(), Exists), "textura principal obrigatória");
        Throws<ArgumentException>(() =>
            loader.Register("mod-c", new InvalidPathHair(), Exists), "recusa path traversal");
        Throws<ArgumentException>(() =>
            loader.Register("../mod", new ProgressHair(), Exists), "recusa UID inválido");
        Throws<InvalidOperationException>(() =>
            loader.Register("mod-c", new BadGenderHair(), name => true),
            "recusa gênero desconhecido");
        True(loader.GetDefinitions().Count == 2,
            "falhas não consumiram IDs nem registraram parcialmente");
        var broken = new ThrowingHair();
        Throws<InvalidOperationException>(() =>
            loader.Register("mod-c", broken, Exists), "falha em SetStaticDefaults é propagada");
        True(broken.Type == -1 && loader.GetDefinitions().Count == 2,
            "falha nos defaults reverte o Type e não polui o registro");
        True(loader.Register("mod-c", new ProgressHair(), Exists).Type == 230,
            "ID não é consumido por SetStaticDefaults inválido");
        True(!loader.TryResolveSavedHair("mod-c/Absent", out int absent) && absent == 0,
            "mod desabilitado cai no cabelo vanilla");

        loader.Reset();
        True(progress.Type == -1 && alternate.Type == -1, "reset solta tipos antigos");
        True(loader.GetDefinitions().Count == 0, "reset limpa referências da sessão");
        True(loader.Register("mod-a", new ProgressHair(), Exists).Type == 228,
            "ID inicial é restaurado após reset");

        var custom = new HairLoader(300);
        True(custom.Register("mod-a", new ProgressHair(), Exists).Type == 300,
            "suporta primeiro ID nativo parametrizado");
        custom.Reset();
        True(custom.Register("mod-a", new ProgressHair(), Exists).Type == 300,
            "reset preserva base configurada");
        Throws<ArgumentOutOfRangeException>(() => new HairLoader(227),
            "não invade intervalo de cabelos vanilla");

        var prepared = HairManifestSession.Prepare("mod-test",
            new[] { typeof(ProgressHair), typeof(string), typeof(ModHair), typeof(AltHair) },
            Exists, 300);
        True(prepared.Count == 2, "descoberta ignora tipos nao ModHair e classe abstrata");
        True(prepared.Loader.TryResolveSavedHair("mod-test/AltHair", out int preparedAlt) &&
             preparedAlt == 300, "ordem de registro C# independe da ordem da reflexao");
        True(prepared.Loader.TryResolveSavedHair("mod-test/ProgressHair", out int preparedProgress) &&
             preparedProgress == 301, "segundo ModHair recebe o ID sequencial correto");
        True(prepared.Manifest.StartsWith(
            "BUNNY_HAIR_MANIFEST_V1\\nHAIR\\t300\\tmod-test\\tAltHair\\t",
            StringComparison.Ordinal), "manifesto da sessao inicia no ID nativo reservado");
        True(prepared.Manifest.Contains(
            "HAIR\\t301\\tmod-test\\tProgressHair\\t", StringComparison.Ordinal),
            "manifesto de varias classes respeita ordem deterministica");

        Throws<InvalidOperationException>(() => HairManifestSession.Prepare(
            "mod-test", new[] { typeof(ProgressHair), typeof(ThrowingHair) }, Exists),
            "um SetStaticDefaults falho rejeita a sessao inteira");
        Throws<InvalidOperationException>(() => HairManifestSession.Prepare(
            "mod-test", new[] { typeof(ProgressHair), typeof(NoTextureHair) }, Exists),
            "asset ausente rejeita a sessao inteira");
        Throws<InvalidOperationException>(() => HairManifestSession.Prepare(
            "mod-test", new[] { typeof(MissingConstructorHair) }, Exists),
            "ModHair sem construtor publico vazio e recusado");
        Throws<InvalidOperationException>(() => HairManifestSession.Prepare(
            "mod-test", new[] { typeof(DuplicateNameA), typeof(DuplicateNameB) },
            name => true), "identidades estaveis duplicadas entre namespaces sao recusadas");
        Throws<InvalidOperationException>(() => HairManifestSession.Prepare(
            "mod-test", new[] { typeof(string), typeof(ModHair) }, Exists),
            "sessao sem classe ModHair concreta falha explicitamente");

        Console.WriteLine("C# HairLoader: " + passed + " verificações gerenciadas passaram.");
    }
}

public sealed class ProgressHair : ModHair
{
    public bool BossDowned { get; set; }
    public int StaticDefaultsCalls { get; private set; }
    public int AutoDefaultsCalls { get; private set; }
    public int TypeSeenAtStaticDefaults { get; private set; } = -1;
    public int TypeSeenAtAutoDefaults { get; private set; } = -1;

    public override void AutoStaticDefaults()
    {
        AutoDefaultsCalls++;
        TypeSeenAtAutoDefaults = Type;
    }
    public override void SetStaticDefaults()
    {
        StaticDefaultsCalls++;
        TypeSeenAtStaticDefaults = Type;
    }

    public override IEnumerable<Condition> GetUnlockConditions()
    {
        yield return new Condition("Derrotar o chefe", () => BossDowned);
    }
}

public sealed class AltHair : BunnyHair
{
    public override bool AvailableDuringCharacterCreation => false;
    public override Gender RandomizedCharacterCreationGender => Gender.Female;
}

public sealed class NoTextureHair : ModHair { }

public sealed class InvalidPathHair : ModHair
{
    public override string Texture => "../outside";
}

public sealed class BadGenderHair : ModHair
{
    public override Gender RandomizedCharacterCreationGender => (Gender)89;
}

public sealed class ThrowingHair : ModHair
{
    public override void SetStaticDefaults() =>
        throw new InvalidOperationException("Falha intencional do exemplo.");
}

public sealed class MissingConstructorHair : ModHair
{
    public MissingConstructorHair(int unused) { }
}

public sealed class DuplicateNameA : ModHair
{
    public override string Name => "SharedHair";
}

public sealed class DuplicateNameB : ModHair
{
    public override string Name => "SharedHair";
}
