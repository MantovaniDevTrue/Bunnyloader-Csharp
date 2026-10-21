using BunnyLoader.Source;

int passed = 0;
void Check(string name, Action action)
{
    action();
    Console.WriteLine("PASS " + name);
    passed++;
}
void Is(bool condition, string message)
{
    if (!condition) throw new Exception(message);
}
void Throws<T>(Action action) where T : Exception
{
    try { action(); } catch (T) { return; }
    throw new Exception("Excecao esperada: " + typeof(T).Name);
}

Check("command context carries registered UID", () => {
    var owned = new BunnyCommandRegistry();
    var replies = new List<BunnyCommandReply>();
    owned.Register("different-uid", new[] { new OwnerCommand() });
    Is(owned.TryHandle("/owner", new BunnyCommandCaller(0,0,BunnyCommandCallerKind.Chat,replies.Add)), "owner not handled");
    Is(replies.Single().Text == "different-uid", "caller supplied or hardcoded mod identity");
});
var output = new List<BunnyCommandReply>();
BunnyCommandCaller Caller(int mode = 0, BunnyCommandCallerKind kind = BunnyCommandCallerKind.Chat)
    => new(0, mode, kind, output.Add);
var errors = new List<Exception>();
var registry = new BunnyCommandRegistry(errors.Add);
var echo = new Echo();
registry.Register("examplemod", new BunnyCommand[] { echo });
Check("chamada direta", () => {
    Is(registry.TryHandle("/echo UM Dois", Caller()), "nao reconheceu");
    Is(echo.LastInput == "/echo um dois", "case insensitive");
    Is(echo.LastArgs.SequenceEqual(new[] { "um", "dois" }), "args");
});
Check("alias e espacos", () => {
    Is(registry.TryHandle("  /dizer   Ola   MUNDO  ", Caller()), "alias nao reconhecido");
    Is(echo.LastArgs.SequenceEqual(new[] { "ola", "mundo" }), "separacao");
});
Check("o vanilla continua quando nao reconhecido", () => {
    Is(!registry.TryHandle("/help", Caller()), "interceptou /help");
    Is(!registry.TryHandle("mensagem normal", Caller()), "interceptou texto");
    Is(!registry.TryHandle("/x:y", Caller()), "interceptou nome sem dono");
});
Check("resposta dividida por linha", () => {
    output.Clear();
    Caller().Reply("linha 1\nlinha 2");
    Is(output.Count == 2 && output[1].Text == "linha 2", "newline");
});
Check("mundo local", () => {
    var world = new BunnyCommandRegistry();
    world.Register("worldmod", new[] { new World() });
    Is(world.TryHandle("/time", Caller()), "singleplayer");
    Is(!world.TryHandle("/time", Caller(1)), "cliente executou World");
    Is(world.TryHandle("/time", Caller(2, BunnyCommandCallerKind.Server)), "dedicado");
    Is(world.TryHandle("/time", Caller(3, BunnyCommandCallerKind.Server)), "host");
    Is(!world.TryHandle("/time", Caller(3)), "host chat executou World");
});
Check("server nao roda no cliente", () => {
    var server = new BunnyCommandRegistry();
    server.Register("servermod", new[] { new Server() });
    Is(!server.TryHandle("/admin", Caller(1)), "executou no cliente");
    Is(server.TryHandle("/admin", Caller(2, BunnyCommandCallerKind.Server)), "nao executou no servidor");
});
Check("ajuda por tipo", () => {
    Is(registry.GetHelp(Caller()).Count == 1, "help nao listado");
    Is(registry.GetHelp(Caller(2, BunnyCommandCallerKind.Server)).Count == 0, "vazou comando Chat");
});
Check("conflito entre mods exige namespace", () => {
    registry.Register("another", new[] { new Echo() });
    output.Clear();
    Is(registry.TryHandle("/echo x", Caller()), "conflito nao consumido");
    Is(output.Any(x => x.Text.Contains("examplemod:echo")), "sem desambiguacao");
    output.Clear();
    Is(registry.TryHandle("/another:echo X", Caller()), "comando qualificado");
    Is(!output.Any(x => x.Text.Contains("mais de um mod")), "qualificado ambiguo");
});
Check("mod qualificado inexistente responde erro", () => {
    output.Clear();
    Is(registry.TryHandle("/absent:echo", Caller()), "erro deveria consumir");
    Is(output.Count == 1 && output[0].Text.Contains("absent"), "sem erro do dono");
});
Check("unload remove somente o mod", () => {
    registry.Unregister("another");
    output.Clear();
    Is(registry.TryHandle("/echo Retorno", Caller()), "alias depois unload");
    Is(!output.Any(x => x.Text.Contains("mais de um mod")), "sobrou conflito");
    registry.Unregister("examplemod");
    Is(!registry.TryHandle("/echo", Caller()), "comando sobreviveu unload");
});
Check("descoberta de comandos exportados", () => {
    var discovered = new BunnyCommandRegistry();
    Is(discovered.RegisterTypes("types", new[] { typeof(SamplePublicCommand) }) == 1, "registro por classe");
    Is(discovered.TryHandle("/auto", Caller()), "classe nao registrada");
    discovered.Unregister("types");
    Is(!discovered.TryHandle("/auto", Caller()), "registro nao descarregado");
    Is(discovered.RegisterAssembly("assembly", typeof(SamplePublicCommand).Assembly) == 1,
        "montagem deveria incluir apenas a classe publica exportada");
    Is(discovered.TryHandle("/auto", Caller()), "montagem nao registrada");
});
Check("registro invalido nao altera o registro", () => {
    Throws<ArgumentException>(() => registry.Register("badmod", new BunnyCommand[] { new Invalid() }));
    Is(!registry.TryHandle("/ok", Caller()), "rollback falhou");
    Throws<ArgumentException>(() => registry.Register("bad mod", new[] { new Echo() }));
    Throws<ArgumentOutOfRangeException>(() => registry.Register("zero", Array.Empty<BunnyCommand>()));
});
Check("duplicatas no mesmo mod", () => {
    Throws<ArgumentException>(() => registry.Register("dup", new BunnyCommand[] { new Echo(), new Echo() }));
    Is(!registry.TryHandle("/echo", Caller()), "registro parcialmente instalado");
});
Check("erros de uso geram resposta", () => {
    var command = new BunnyCommandRegistry();
    command.Register("err", new[] { new Usage() });
    output.Clear();
    Is(command.TryHandle("/usage", Caller()), "uso nao consumido");
    Is(output.Count == 1 && output[0].Text == "Falta argumento", "mensagem usage");
});
Check("excecao nao passa para thread do jogo", () => {
    var command = new BunnyCommandRegistry(errors.Add);
    command.Register("bad", new[] { new Broken() });
    output.Clear();
    Is(command.TryHandle("/broken", Caller()), "falha nao consumida");
    Is(errors.Count == 1 && output.Count == 1 && output[0].Text == "Uso: /broken", "isolation");
});
Check("case sensitive preserva argumentos", () => {
    var command = new BunnyCommandRegistry();
    var sensitive = new Sensitive();
    command.Register("case", new[] { sensitive });
    Is(command.TryHandle("/sensitive MaiUsCULa", Caller()), "sensivel");
    Is(sensitive.Last == "MaiUsCULa", "argumento perdeu case");
});
Check("limites de seguranca", () => {
    Is(!registry.TryHandle("/" + new string('a', BunnyCommandRegistry.MaxInputLength + 1), Caller()), "input enorme");
    Throws<ArgumentException>(() => registry.Register("bad", new[] { new BadName() }));
    Throws<ArgumentException>(() => registry.Register("bad", new[] { new EmptyAlias() }));
    Throws<ArgumentOutOfRangeException>(() => new BunnyCommandCaller(256, 0, BunnyCommandCallerKind.Chat, output.Add));
});

Console.WriteLine("COMMAND_HOST_TESTS_PASS=" + passed);

sealed class Echo : BunnyCommand
{
    public override string Command => "echo";
    public override IReadOnlyList<string> Aliases => new[] { "dizer" };
    public override string Description => "Ecoa texto";
    public string LastInput = "";
    public string[] LastArgs = Array.Empty<string>();
    public override void Action(BunnyCommandContext context)
    {
        LastInput = context.Input;
        LastArgs = context.Arguments.ToArray();
        context.Caller.Reply(string.Join(" ", context.Arguments));
    }
}
sealed class World : BunnyCommand
{
    public override string Command => "time";
    public override BunnyCommandType Type => BunnyCommandType.World;
    public override void Action(BunnyCommandContext context) => context.Caller.Reply("world");
}
sealed class Server : BunnyCommand
{
    public override string Command => "admin";
    public override BunnyCommandType Type => BunnyCommandType.Server;
    public override void Action(BunnyCommandContext context) => context.Caller.Reply("server");
}
sealed class Invalid : BunnyCommand
{
    public override string Command => "ok";
    public override IReadOnlyList<string> Aliases => new[] { "ok" };
    public override void Action(BunnyCommandContext context) { }
}
sealed class Usage : BunnyCommand
{
    public override string Command => "usage";
    public override void Action(BunnyCommandContext context) => throw new BunnyUsageException("Falta argumento");
}
sealed class Broken : BunnyCommand
{
    public override string Command => "broken";
    public override void Action(BunnyCommandContext context) => throw new InvalidOperationException("Falha intencional");
}
sealed class Sensitive : BunnyCommand
{
    public override string Command => "sensitive";
    public override bool IsCaseSensitive => true;
    public string Last = "";
    public override void Action(BunnyCommandContext context) => Last = context.Arguments[0];
}
sealed class BadName : BunnyCommand
{
    public override string Command => "nome invalido";
    public override void Action(BunnyCommandContext context) { }
}
sealed class EmptyAlias : BunnyCommand
{
    public override string Command => "valid";
    public override IReadOnlyList<string> Aliases => new[] { "" };
    public override void Action(BunnyCommandContext context) { }
}

public sealed class SamplePublicCommand : BunnyCommand
{
    public override string Command => "auto";
    public override void Action(BunnyCommandContext context) => context.Caller.Reply("registrado");
}

sealed class OwnerCommand : BunnyCommand {
    public override string Command => "owner";
    public override void Action(BunnyCommandContext context) => context.Caller.Reply(context.ModId);
}
