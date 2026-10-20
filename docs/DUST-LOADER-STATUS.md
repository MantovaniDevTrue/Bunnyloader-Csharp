# DustLoader C# — implementação em andamento

Referência nativa: `JonataOliveiraa/Bunny-Loader` (`ModDusts.h/.cpp`). Integração C# baseada somente nas fontes Bunny e na SDK do projeto; sem adaptar APIs tModLoader ou executar callbacks pelo QuickJS.

## Código implementado

- `BunnyDust`, `BunnyDustRegistry` e `BunnyDustState` com ABI validada de 64 bytes, registro por nome/ID, geração de slots e proteção contra falhas repetidas.
- `BunnyDustSourceDispatcher`: descoberta das classes da assembly, vínculo de IDs nativos e callbacks separados por fase `Spawn`, `Update`, `MidUpdate`, `Alpha`, `PreDraw` e `Release`.
- `SourceParser`, `SourceLoader` e Roslyn: domínio `DUST`, verificação do caminho PNG, `UpdateType`, máscara de hooks e exportação gerenciada de defaults.
- `ManagedRuntime`: registro C# no worker MonoVM residente; fila de resultados por fase; marshal exclusivamente de `ManagedDustState` (nunca ponteiros de objetos IL2CPP para worker).
- `ManagedDustMailbox`: até 128 jobs enfileirados, armazenamento separado por fase e slot, prazo máximo de 3 frames, verificação de mod, geração, fase, sessão e frame; respostas inválidas são rejeitadas.
- `ModDusts.cpp`: hooks nativos `NewDust`, `UpdateDust`, `MidUpdate`, `DrawDust` e `GetAlpha` encaminham C# pelo worker quando o tipo pertence à categoria gerenciada; seguem usando `SetNotActive` para devolver as vagas ao pool.
- `mergeManagedDustDelta`: aplica apenas mudanças retornadas pelo C#, em vez de restaurar o snapshot antigo e causar saltos visuais; teste de finitude e identidade de partícula.
- Atualização do registro de máscaras no thread do jogo, inclusive instalação tardia de GetAlpha, sem instalar hooks duplos.
- `noteAutoWorldTilesLoaded` solicita reset da sessão, fila e estado gerenciado quando um mundo é carregado.
- Exemplo `ExampleSparkDust` isolado em `DustLoaderPreview`. Foi retirado de `Playable` porque a feature ainda está bloqueada e não pode comprometer o ExampleMod C# estável.

## Validações concluídas

- Verificações de contrato C# e C++ e teste da ABI de 64 bytes.
- Smoke da compilação de fonte via Roslyn, exportação do registro DUST e execução dos callbacks por ponteiro em C#.
- Testes da mailbox para concorrência entre fases, expiração, troca de mundo, reincidência, descarte e reuso da vaga.
- Testes matemáticos do merge de delta: posição, escala, alpha, cor, cancelamento e NaN.
- Compilação nativa ARM64 em CI sem empacotamento de APK, com execuções aprovadas; a última execução após alterações adicionais deve ser avaliada separadamente.
- As DLLs `BunnySourceSdk.dll` e `BunnySourceCompiler.dll` são montadas pelo pipeline C# sem APK, e os hashes correspondentes são reescritos em `framework.sha256` no mesmo commit.

## Bloqueios antes de considerar completo

1. **A barreira `managedDustCallbacksConnected()` continua em `false`.** Mods C# que sobrescrevem callbacks Dust continuam recusados com `MonoDustBridgeNotConnected`. Isso evita anunciar funcionalidade não validada e mantém o ExampleMod estável.
2. Ainda falta executar o fluxo completo em Terraria Android com IL2CPP e MonoVM juntos, conferindo campos nativos como `dustIndex`, `position`, `scale`, `rotation`, `alpha`, `color` e `frame`.
3. Validar em jogo a fidelidade do primeiro `OnSpawn`, de `PreDraw`, do `GetAlpha`, do retorno `Update=false`, da morte e devolução de slots. O worker assíncrono aplica resultados em frames posteriores (até 3); é necessário avaliar e ajustar esse atraso visual.
4. Medir desempenho, consumo de memória, limite de 128 jobs, estabilidade em grandes lotes, comportamento com QuickJS + vários mods C# e recarregamento de mundos.
5. A versão compilada das DLLs precisa ter seu hash e build nativa revalidados na versão final do commit. Nenhuma falha de compilação ou regressão em mods existentes pode ser aceita.

## Regra permanente de release

**Não gerar APK, não mesclar a PR draft e não anunciar o DustLoader como completo até fechar todos os bloqueadores.** Os pipelines permitidos geram apenas objetos nativos temporários e DLLs gerenciadas. A checagem de UI/Android não foi executada nesta etapa.
