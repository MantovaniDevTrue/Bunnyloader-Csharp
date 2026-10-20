#pragma once

namespace bl::csharp::touch {

// Tenta instalar apenas o observador da atualização do controlador.
// Não libera host_ready nem registra ações no editor: isso depende da
// integração completa das tabelas e dos layouts do Terraria.
void pumpNativeTouchObserverOnGameThread();
bool nativeTouchObserverInstalled();

} // namespace bl::csharp::touch
