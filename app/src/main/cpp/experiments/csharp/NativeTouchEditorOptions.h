#pragma once
namespace bl::csharp::touch {

// Injeta apenas os IDs ativos no seletor do editor. Nunca executa enquanto
// as cinco tabelas do controlador não foram validadas e sincronizadas.
bool ensureNativeTouchEditorOptionsOnGameThread();

} // namespace bl::csharp::touch
