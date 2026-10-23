using System;

namespace BunnyLoader.Source;

/// <summary>
/// Ponte de entrada de um mod C# para os controles do Terraria Mobile.
/// O loader encontra esta classe pelo namespace e pelo nome BunnyTouchEntry:
/// Load roda uma vez no trabalhador MonoVM, e ProcessTriggers uma vez por
/// GameUpdateCount ativo, sem depender de classes ou códigos JavaScript.
/// </summary>
public static class BunnyTouchEntry
{
    private static BunnyTouchButton? randomBuff;
    private static BunnyTouchButton? learningExample;
    private static int heldTicks;
    private static int doubleTapWindow;

    public static int RandomBuffPresses { get; private set; }
    public static int DoubleTaps { get; private set; }

    public static void Load()
    {
        // O nome completo, por exemplo examplemodcsharp/RandomBuff, também
        // é a identidade salva no perfil nativo de controles do jogador.
        randomBuff = BunnyTouchButtonLoader.RegisterButton(
            "examplemodcsharp", "RandomBuff",
            new BunnyTouchButtonOptions
            {
                Label = "Buff aleatório",
                Icon = "Assets/TouchButtons/RandomBuff.png"
            });
        learningExample = BunnyTouchButtonLoader.RegisterButton(
            "examplemodcsharp", "LearningExample",
            new BunnyTouchButtonOptions
            {
                Label = "Aprender toque",
                Icon = "Assets/TouchButtons/LearningExample.png"
            });
    }

    public static void ProcessTriggers()
    {
        // JustPressed acontece uma única vez por toque. Um botão de grupo
        // segue exatamente o mesmo estado que um botão individual.
        if (randomBuff?.JustPressed == true)
        {
            RandomBuffPresses++;
            Console.WriteLine("RandomBuff: toque " + RandomBuffPresses);
        }

        // Current vale em todos os frames nos quais o botão permanece
        // pressionado; RetroCurrent bloqueia chat, placa e diálogo de NPC.
        if (learningExample?.RetroCurrent == true)
        {
            heldTicks++;
            if (heldTicks == 30)
                Console.WriteLine("LearningExample: pressionado por 30 ticks");
        }
        else
        {
            heldTicks = 0;
        }

        if (doubleTapWindow > 0) doubleTapWindow--;
        if (learningExample?.JustPressed == true)
        {
            if (doubleTapWindow > 0)
            {
                DoubleTaps++;
                doubleTapWindow = 0;
                Console.WriteLine("LearningExample: toque duplo " + DoubleTaps);
            }
            else
            {
                doubleTapWindow = 15;
            }
        }
    }
}
