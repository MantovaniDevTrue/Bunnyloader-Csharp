namespace FiveLoadersCSharp;

// Estado transitório dos controles. É intencionalmente separado do progresso
// salvo: pressionar um botão de interface não modifica um mundo multiplayer.
public static class QaTouch
{
    public static int Presses, Holds, DoubleTaps;
}
