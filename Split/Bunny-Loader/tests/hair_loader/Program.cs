using BunnyLoader.Source;

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
        True(registered.MainTexture == registered.AltTexture, "textura alternativa ausente usa a principal");
        True(progress.Type == registered.Type, "ID chega à instância C#");
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
        True(second.RandomGender == HairGender.Female,
            "gênero aleatório compatível");
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

        Console.WriteLine("C# HairLoader: " + passed + " verificações gerenciadas passaram.");
    }
}

public sealed class ProgressHair : ModHair
{
    public bool BossDowned { get; set; }
    public int StaticDefaultsCalls { get; private set; }

    public override void SetStaticDefaults() => StaticDefaultsCalls++;

    public override IEnumerable<Condition> GetUnlockConditions()
    {
        yield return new Condition("Derrotar o chefe", () => BossDowned);
    }
}

public sealed class AltHair : BunnyHair
{
    public override bool AvailableDuringCharacterCreation => false;
    public override HairGender RandomizedCharacterCreationGender => HairGender.Female;
}

public sealed class NoTextureHair : ModHair { }

public sealed class InvalidPathHair : ModHair
{
    public override string Texture => "../outside";
}

public sealed class BadGenderHair : ModHair
{
    public override HairGender RandomizedCharacterCreationGender => (HairGender)89;
}

Check.Run();
