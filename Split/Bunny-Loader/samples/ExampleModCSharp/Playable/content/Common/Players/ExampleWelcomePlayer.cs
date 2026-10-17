using BunnyLoader.Source;

namespace ExampleModCSharp;

public sealed class ExampleWelcomePlayer : BunnyPlayer
{
    public override void SetDefaults()
    {
    }

    public override void UpdateEquips(bool accessoryEquipped)
    {
    }

    public override string OnEnterWorld()
    {
        return "C# Hook QA 0.1.44: "
            + "H1 " + ExampleHookInt32Zero.Result()
            + " | H2 " + ExampleHookVoidInt32.Result()
            + " | H3 " + ExampleHookVoidInt32Pair.Result()
            + " | H4 " + ExampleHookInt32Triple.Result()
            + " | H5 " + ExampleHookVoidInt32Triple.Result();
    }

    public override string OnRespawn()
    {
        return "Example C#: BunnyPlayer OnRespawn ativo!";
    }

    public override bool PreUpdate()
    {
        return true;
    }

    public override bool PostUpdate()
    {
        return true;
    }

    public override string UpdateDead()
    {
        return "Example C#: BunnyPlayer UpdateDead ativo!";
    }
}

