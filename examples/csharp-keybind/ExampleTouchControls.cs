using BunnyLoader.Source;

namespace ExampleKeybind;

/// <summary>
/// Exemplo do botão RandomBuff do ExampleModBunny, em C# puro.
/// Não é carregado automaticamente: o mod deve chamar Load no seu ciclo de vida.
/// O host Android precisa oferecer o botão no editor antes do teste em jogo.
/// </summary>
public static class ExampleTouchControls
{
    private static BunnyTouchButton? randomBuff;

    public static void Load()
    {
        // O "P" é a tecla sugerida no PC; no Terraria mobile usamos o
        // botão de toque com identidade persistente "examplemodcsharp/RandomBuff".
        randomBuff = BunnyTouchButtonLoader.RegisterKeybind(
            "examplemodcsharp", "RandomBuff", "P");
        // O registro nativo acontece antes do editor ficar pronto.
        // O método não libera gameplay enquanto todos os hooks faltantes
        // não passarem na validação de ABI.
        BunnyTouchButtonLoader.TryBindNativeHost();
    }

    public static bool WantsRandomBuff()
    {
        // Chamado pelo ProcessTriggers do ModPlayer. Após o hook ficar
        // pronto, a ponte é ativada sem depender de JavaScript.
        BunnyTouchButtonLoader.TryBindNativeHost();
        return randomBuff?.JustPressed == true;
    }

    public static bool CanUseInGameplay()
    {
        // RetroCurrent fica falso ao abrir chat, diálogo ou placa.
        return randomBuff?.RetroCurrent == true;
    }

    public static void Unload()
    {
        BunnyTouchButtonLoader.UnregisterMod("examplemodcsharp");
        randomBuff = null;
    }
}
