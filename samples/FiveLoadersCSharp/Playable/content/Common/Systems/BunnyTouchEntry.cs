using System.Threading;
using FiveLoadersCSharp;

namespace BunnyLoader.Source;

// Contrato de descoberta do loader: namespace e nome exatos; Load uma vez,
// ProcessTriggers na thread Mono para cada atualização ativa do jogo.
public static class BunnyTouchEntry
{
    private static BunnyTouchButton? button;
    private static int held, doubleWindow, triggerFrames, boundFrames;
    public static void Load()
    {
        button = BunnyTouchButtonLoader.RegisterButton(
            "fd8b8d75-62c9-4ae9-9000-45d9be591501", "TestarLote",
            new BunnyTouchButtonOptions { Label = "QA dos cinco loaders", Icon = "Assets/TouchButtons/Qa.png" });
    }
    public static string Diagnostics()
    {
        BunnyTouchButton? current = button;
        if (current is null) return "registro=ausente";
        BunnyTouchButtonState state = current.State;
        return "registro=ok host=" + BunnyTouchButtonLoader.LastHostError +
            " bound=" + state.IsBound + " current=" + state.Current +
            " triggerFrames=" + Volatile.Read(ref triggerFrames) +
            " boundFrames=" + Volatile.Read(ref boundFrames);
    }
    public static void ProcessTriggers()
    {
        Interlocked.Increment(ref triggerFrames);
        if (button?.IsBound == true) Interlocked.Increment(ref boundFrames);
        if (doubleWindow > 0) doubleWindow--;
        if (button?.JustPressed == true)
        {
            Interlocked.Increment(ref QaTouch.Presses);
            if (doubleWindow > 0) { Interlocked.Increment(ref QaTouch.DoubleTaps); doubleWindow = 0; }
            else doubleWindow = 15;
        }
        // RetroCurrent bloqueia ação durante chat, placas e diálogo de NPC.
        if (button?.RetroCurrent == true)
        {
            if (++held == 30) Interlocked.Increment(ref QaTouch.Holds);
        }
        else held = 0;
    }
}
