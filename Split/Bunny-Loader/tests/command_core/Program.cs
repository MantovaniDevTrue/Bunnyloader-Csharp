using BunnyLoader.Source;
using BunnyLoader.CommandTests;

static void Check(bool value, string description)
{
    if (!value) throw new InvalidOperationException(description);
}

var world = new FakeWorld();
var player = new FakePlayer();
var replies = new List<(string Text, uint Color)>();
var failures = new List<string>();
var dispatcher = new BunnyCommandDispatcher(failures.Add);
BunnyCommandCaller Caller(BunnyCommandType type) =>
    new(type, player, world, (text, color) => replies.Add((text, color)),
        (key, fallback) => fallback);

dispatcher.Register("examplemodcsharp", new ExampleHealCommand());
dispatcher.Register("examplemodcsharp", new ExampleTimeCommand());
Check(dispatcher.Count == 2, "registration");
Check(!dispatcher.Handle("hello", Caller(BunnyCommandType.Chat)), "non-command is vanilla");
Check(!dispatcher.Handle("/unrecognized 123", Caller(BunnyCommandType.Chat)), "unknown is vanilla");
Check(dispatcher.Handle("/curar 10.9", Caller(BunnyCommandType.Chat)), "alias");
Check(player.Life == 60 && player.LastEffect == 10, "heal clamps decimal toward floor");
Check(replies[^1].Color == BunnyCommandColors.Success, "healing reply color");
Check(dispatcher.Handle("/HEAL 100", Caller(BunnyCommandType.Chat)), "case-insensitive command");
Check(player.Life == 100 && player.LastEffect == 40, "heal clamp to max health");
Check(dispatcher.Handle("/heal 5", Caller(BunnyCommandType.Chat)), "full health handled");
Check(replies[^1].Text.Contains("cheia"), "full health message");
Check(dispatcher.Handle("/heal -2", Caller(BunnyCommandType.Chat)), "bad argument handled");
Check(replies[^1].Color == BunnyCommandColors.Error, "usage error");
Check(dispatcher.Handle("/heal xyz", Caller(BunnyCommandType.Chat)), "non number handled");
world.Time = 53900;
world.DayTime = true;
world.NetMode = 0;
Check(dispatcher.Handle("/addtime 300", Caller(BunnyCommandType.Chat)), "solo World command");
Check(!world.DayTime && Math.Abs(world.Time - 200) < 0.001, "day to night");
Check(world.Broadcasts == 0, "solo has no network broadcast");
Check(dispatcher.Handle("/addtime -600", Caller(BunnyCommandType.Chat)), "negative ticks");
Check(world.DayTime && Math.Abs(world.Time - 53600) < 0.001, "night to day");
world.NetMode = 1;
Check(!dispatcher.Handle("/addtime 5", Caller(BunnyCommandType.Chat)), "client cannot change world");
world.NetMode = 3;
Check(!dispatcher.Handle("/addtime 5", Caller(BunnyCommandType.Chat)), "host chat route is not server route");
Check(dispatcher.Handle("/addtime 5", Caller(BunnyCommandType.Server)), "host server World route");
Check(world.Broadcasts == 1, "host synchronizes WorldData");
Check(dispatcher.Handle("/heal 2", Caller(BunnyCommandType.Chat)), "host local chat command");
Check(!dispatcher.Handle("/heal 2", Caller(BunnyCommandType.Server)), "heal stays Chat-only");

var help = dispatcher.GetHelp(BunnyCommandType.Chat, 0);
Check(help.Count == 2 && help.Any(h => h.Command == "heal") &&
    help.Any(h => h.Command == "addtime"), "/help solo");
Check(dispatcher.GetHelp(BunnyCommandType.Chat, 1).Count == 1, "/help client");
Check(dispatcher.GetHelp(BunnyCommandType.Server, 3).Count == 1, "/help server");

var second = new BunnyCommandDispatcher();
second.Register("moda", new ExampleHealCommand());
second.Register("modb", new ExampleHealCommand());
Check(second.Handle("/heal 2", Caller(BunnyCommandType.Chat)), "ambiguous command consumed");
Check(replies[^1].Text.Contains("/modb:heal"), "ambiguity suggests mod namespace");
player.Life = 50;
Check(second.Handle("/modb:heal 9", Caller(BunnyCommandType.Chat)) &&
    player.Life == 59, "explicit mod dispatch");
Check(second.Handle("/modc:heal 9", Caller(BunnyCommandType.Chat)), "wrong mod reports error");
second.UnregisterMod("moda");
Check(second.Count == 1 && second.Handle("/heal 1", Caller(BunnyCommandType.Chat)),
    "mod unload frees aliases without breaking other mods");
second.Clear();
Check(second.Count == 0 && !second.Handle("/heal 1", Caller(BunnyCommandType.Chat)),
    "session shutdown clears commands");

bool duplicate = false;
try { dispatcher.Register("examplemodcsharp", new ExampleHealCommand()); }
catch (InvalidOperationException) { duplicate = true; }
Check(duplicate && dispatcher.Count == 2, "duplicate registration is atomic");

dispatcher.Register("faultmod", new FaultCommand());
Check(dispatcher.Handle("/broken", Caller(BunnyCommandType.Chat)), "fault isolated");
Check(failures.Count == 1 && replies[^1].Color == BunnyCommandColors.Error,
    "exception logged and converted to usage");
Check(dispatcher.Handle("/heal 1", Caller(BunnyCommandType.Chat)),
    "healthy commands survive another mod fault");

Console.WriteLine("BUNNY_CSHARP_COMMAND_CORE=PASS");

public sealed class FakePlayer : IBunnyCommandPlayer
{
    public int Life { get; set; } = 50;
    public int MaximumLife => 100;
    public int LastEffect { get; private set; }
    public void HealEffect(int amount) => LastEffect = amount;
}

public sealed class FakeWorld : IBunnyCommandWorld
{
    public int NetMode { get; set; }
    public bool DayTime { get; set; } = true;
    public double Time { get; set; }
    public double DayLength => 54000;
    public double NightLength => 32400;
    public int Broadcasts { get; private set; }
    public void SetClock(bool dayTime, double time) { DayTime = dayTime; Time = time; }
    public void BroadcastWorldData() => Broadcasts++;
}

public sealed class FaultCommand : BunnyCommand
{
    public override string Command => "broken";
    public override bool Action(BunnyCommandCaller caller, string input, IReadOnlyList<string> args)
        => throw new InvalidOperationException("expected test failure");
}
