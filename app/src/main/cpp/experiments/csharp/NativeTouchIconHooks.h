#pragma once
namespace bl::csharp::touch {

// Intercepta apenas ícones de ações C#; nomes e botões vanilla seguem intactos.
// Falta validar o hook QuickActionButton.Draw para liberar o editor no jogo.
void pumpNativeTouchIconFrameHooksOnGameThread();
bool nativeTouchIconFrameHooksInstalled();
// Hook de desenho próprio: registra-se somente com Texture2D pré-carregada.
void pumpNativeTouchQuickDrawHookOnGameThread();
bool nativeTouchQuickDrawHookInstalled();

} // namespace bl::csharp::touch
