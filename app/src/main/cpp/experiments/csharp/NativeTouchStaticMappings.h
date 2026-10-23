#pragma once
namespace bl::csharp::touch {

// Atualiza somente as tabelas nativas dos enums de ação de controles.
// Verifica todos os tipos e colisões ANTES de publicar qualquer array.
bool ensureNativeTouchStaticMappingsOnGameThread();
void resetNativeTouchStaticMappingSession();

} // namespace bl::csharp::touch
