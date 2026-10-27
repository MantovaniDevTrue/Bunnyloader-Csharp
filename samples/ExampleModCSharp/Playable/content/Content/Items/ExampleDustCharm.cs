using System;
using BunnyLoader.Source;
using ExampleModCSharp.Content.Effects;

namespace ExampleModCSharp;

// Este item exercita a poeira C# real. O Mono envia valores; o jogo cria as
// partículas na própria thread, sem acessar memória do Terraria pelo callback.
public sealed class ExampleDustCharm : BunnyItem
{
    public override void SetDefaults()
    {
        Texture = "Content/Items/ExampleDustCharm.png";
        DisplayName = "Example Dust Charm";
        Tooltip = "Creates Example Spark Dust to test the C# DustLoader";
        Width = 20;
        Height = 20;
        MaxStack = 1;
        UseTime = 30;
        UseAnimation = 30;
        UseStyle = 4;
    }

    public override bool OnUse(int playerIndex)
    {
        // A fila é limitada: false significa que não foi aceito, nunca sucesso fictício.
        if (!BunnyDustEffects.EmitAtPlayer<ExampleSparkDust>(playerIndex))
            throw new InvalidOperationException("ExampleDustCharm: emission queue unavailable or world inactive");
        return true;
    }

    public override void AddRecipes() => CreateRecipe().AddIngredient<ExampleItem>(1).Register();
}
