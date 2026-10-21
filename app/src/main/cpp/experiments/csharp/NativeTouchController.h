#pragma once

namespace bl::csharp::touch {

// Tenta instalar apenas o observador da atualização do controlador.
// Não libera host_ready nem registra ações no editor: isso depende da
// integração completa das tabelas e dos layouts do Terraria.
void pumpNativeTouchObserverOnGameThread();
bool nativeTouchObserverInstalled();
// IDs reais são descobertos por reflexão e reservados fora do espaço JS.
// -1 quando o controlador nativo não está instalado.
int nativeTouchActionOf(int id);
int nativeTouchControlOf(int id);
int nativeTouchQuickOf(int id);
int nativeTouchIndexByAction(int raw);

} // namespace bl::csharp::touch
