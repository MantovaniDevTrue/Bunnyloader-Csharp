#pragma once
namespace bl::csharp::touch {

// Instala apenas GetControlName depois de validar System.String, parâmetros
// e endereço nativo. Não ativa host_ready nem tenta renderizar ícones.
void pumpNativeTouchEditorLabelsOnGameThread();
bool nativeTouchEditorLabelsInstalled();

} // namespace bl::csharp::touch
