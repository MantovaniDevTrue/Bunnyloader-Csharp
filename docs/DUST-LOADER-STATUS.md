# DustLoader C# — implementação em andamento

Referência nativa: `JonataOliveiraa/Bunny-Loader` (`ModDusts.h/.cpp`). Integração C# baseada somente nas fontes Bunny e na SDK do projeto; sem adaptar APIs tModLoader ou executar callbacks pelo QuickJS.

## Código implementado

- `BunnyDust`, `BunnyDustRegistry` e `BunnyDustState` com ABI validada de 64 bytes, registro por nome/ID, geração de slots e proteção contra falhas repetidas.
- `BunnyDustSourceDispatcher`: descoberta das classes da assembly, vínculo de IDs nativos e callbacks separados por fase `Spawn`, `Update`, `MidUpdate`, `Alpha`, `PreDraw` e `Release`.
- `SourceParser`, `SourceLoader` e Roslyn: domínio `DUST`, verificação do caminho PNG, `UpdateType`, máscara de hooks e exportação gerenciada de defaults.
- `ManagedRuntime`: registro C# no worker MonoVM residente; fila de resultados por fase; marshal exclusivamente de `ManagedDustState` (nunca ponteiros de objetos IL2CPP para worker).
- `ManagedDustMailbox`: até 128 jobs enfileirados, armazenamento separado por fase e slot, prazo máximo de 3 frames, verificação de mod, geração, fase, sessão, quadro e máscara de retorno permitida para cada fase. `Release` cancela pedidos antigos da geração, inclusive os já capturados pelo worker. `OnSpawn` e `Release` têm prioridade sobre os callbacks visuais sob saturação, mantendo `Release` antes de `OnSpawn`.
- `ModDusts.cpp`: hooks nativos `NewDust`, `UpdateDust`, `MidUpdate`, `DrawDust` e `GetAlpha` encaminham C# pelo worker quando o tipo pertence à categoria gerenciada; seguem usando `SetNotActive` para devolver as vagas ao pool.
- `mergeManagedDustDelta`: aplica apenas mudanças retornadas pelo C#, em vez de restaurar o snapshot antigo e causar saltos visuais; teste de finitude e identidade de partícula.
- Atualização do registro de máscaras no thread do jogo, inclusive instalação tardia de GetAlpha, sem instalar hooks duplos.
- `ManagedDustLifecycle`: rastreia gerações e slots observados com remoção O(1), envia `Release` na morte, mudança de tipo ou reutilização, retém liberações para nova tentativa quando a fila está ocupada e prioriza a fase 5. O backlog é limitado a 12.000 liberações; ultrapassar o limite produz erro explícito no log em vez de perda silenciosa.
- `noteAutoWorldTilesLoaded` e a saída de mundo reinicializam o rastreio nativo, o contador de gerações, o cache de respostas, a sessão da mailbox e a instância do dispatcher no MonoVM.
- `BunnyDustSourceDispatcher` impede repetir uma exceção em `OnSpawn` durante a mesma geração; utiliza fallback vanilla e rearma o callback somente em nova geração. `Bind` aceita IDs globais abaixo de 32768 mesmo que o mod tenha registrado poucos tipos localmente. A faixa `332..32767` foi alinhada entre a ABI C++ e o dispatcher C#.
- Estados inativos com `Scale=0` são válidos para encerrar corretamente a partícula. Escala zero enquanto ativa permanece inválida e aciona rollback.
- `ManagedDustDecision` registra o quadro original do pedido, e não o quadro de recebimento: `Update=false`, `MidUpdate` e `PreDraw=false` só são adotados no mesmo quadro; `GetAlpha` também exige a mesma cor de entrada. Deltas visuais recebidos posteriormente continuam independentes das decisões booleanas.
- `ManagedDustFrameBudget` e a `ManagedDustMailbox::await` permitem uma resposta curta no próprio quadro sem executar C# na thread do jogo. Cada tentativa aguarda no máximo 200 µs, com orçamento máximo solicitado de 600 µs por quadro; o kernel pode impor pequenas variações de agendamento. O worker continua no MonoVM, com `OnSpawn` e `Release` prioritários. Ao expirar, o pedido continua para conclusão assíncrona limitada a três quadros, evitando perder callbacks; decisões booleanas atrasadas não se aplicam retroativamente.
- `dustPoolStats()` expõe `managedSyncAttempts`, `managedSyncReady`, `managedSyncMissed` e `managedSyncWaitNs` para medir a eficácia no aparelho sem poluir o log. A tentativa de espera curta só é feita quando o binding do mod está confirmado no worker MonoVM; antes disso, a solicitação segue pelo fluxo assíncrono e preserva o orçamento do quadro.
- `BunnyDustRegistry` aceita IDs reais de mods Bunny C# em formato UUID com hífens, mantendo a validação estrita dos nomes de classes e texturas.
- `ManagedDustSlotLookup` confirma que cada callback atua no objeto exato de `Main.dust`. Usa `Dust.dustIndex` como hint verificado e, quando ausente/incorreto, o mapa nativo de slots; o mapa só é preenchido quando necessário e recebe os slots diretamente em `NewDust`. Reinicializa na troca de mundo.
- Dusts com máscara de callbacks zero permanecem 100% no caminho nativo, sem acordar o MonoVM; somente mods com callbacks Dust armam o worker residente. Notificações do MonoVM são agrupadas na transição da fila vazia para ocupada e para liberações prioritárias. O worker também consulta a fila no modo de auditoria e usa polling de segurança a cada 8 ms quando Dust gerenciado está registrado, eliminando espera indefinida se uma notificação for perdida. A inicialização não se declara concluída se faltar o arquivo de probe.
- O workflow de DLLs recompila SDK e compilador, confere os testes e atualiza `framework.sha256` sem gerar APK. Se a branch receber novos commits, o push é reconciliado somente quando as fontes gerenciadas não mudaram; senão o build antigo encerra como substituído.
- Exemplo `ExampleSparkDust` isolado em `DustLoaderPreview`. Foi retirado de `Playable` porque a feature ainda está bloqueada e não pode comprometer o ExampleMod C# estável.

## Validações concluídas

- Verificações de contrato C# e C++ e teste da ABI de 64 bytes.
- Smoke da compilação de fonte via Roslyn, exportação do registro DUST e execução dos callbacks por ponteiro em C#.
- Testes da mailbox para concorrência entre fases, expiração, troca de mundo, reincidência, descarte e reuso da vaga, invalidação de jobs em voo após `Release`, prioridade sob fila saturada e reaproveitamento seguro da vaga cancelada.
- Testes matemáticos do merge de delta: posição, escala, alpha, cor, cancelamento e NaN.
- Testes novos de `ManagedDustLifecycle`: morte, troca de tipo, reuso de slot, reentrada no mundo, retries, priorização de `Release`, overflows limitados a 12.000, proteção de gerações e estresse em 6.000 slots com remoção O(1).
- Testes C++ adicionais do índice verificado (`dustIndex` como hint), fallback `Main.dust` com objeto incorreto, troca de array e atualização de slots com custo constante; mailbox cobre notificações agrupadas, prioridade de `Release`/`OnSpawn`, consistência de máscaras e rejeição de decisões booleanas atrasadas ou com cor de entrada diferente. Smoke Roslyn exercita também o registro por UUID.
- Teste concorrente de `ManagedDustMailbox::await` exercita conclusão no próprio quadro, timeout com resposta assíncrona posterior, limpeza por `Release`, invalidação por troca de mundo e reinício do orçamento de 600 µs.
- Smoke Roslyn com um `BrokenSpawnDust` confirma que `OnSpawn` defeituoso é chamado apenas uma vez por instância/geração; um novo spawn rearma a chamada.
- Contrato de escala zero valida que somente uma poeira inativa pode terminar com `Scale=0`.
- Compilação nativa ARM64 em CI sem empacotamento de APK aprovada no commit `3f15ee395`; testes C#/C++ também aprovados nesse commit. Cada nova alteração dispara validação; conferir os checks do commit final.
- As DLLs `BunnySourceSdk.dll` e `BunnySourceCompiler.dll` são montadas pelo pipeline C# sem APK, e os hashes correspondentes são reescritos em `framework.sha256` no mesmo commit.

## Bloqueios antes de considerar completo

1. **A barreira `managedDustCallbacksConnected()` continua em `false`.** Mods C# que sobrescrevem callbacks Dust continuam recusados com `MonoDustBridgeNotConnected`. Isso evita anunciar funcionalidade não validada e mantém o ExampleMod estável.
2. Ainda falta executar o fluxo completo em Terraria Android com IL2CPP e MonoVM juntos, conferindo campos nativos como `dustIndex`, `position`, `scale`, `rotation`, `alpha`, `color` e `frame`.
3. **Integração síncrona ainda não garantida:** existe tentativa de despacho no próprio quadro com espera curta e orçamento global. Sob carga, o MonoVM pode não responder no prazo, e a execução continua pelo fallback nativo + atualização assíncrona. Portanto não há equivalência incondicional para `Update=false`, `PreDraw=false` ou `GetAlpha`. Precisamos medir a taxa de respostas no prazo no Android e, se insuficiente, melhorar o agendamento ou reprojetar os callbacks críticos antes de liberar. Validar também a devolução real dos slots pela fila `Release`.
4. Medir desempenho, consumo de memória, limite de 128 jobs, estabilidade em grandes lotes, comportamento com QuickJS + vários mods C# e recarregamento de mundos.
5. Os artefatos devem permanecer com hashes iguais ao manifesto e a compilação ARM64 deve ser aprovada no commit final. Nenhuma falha de compilação ou regressão em mods existentes pode ser aceita.

## Regra permanente de release

**Não gerar APK, não mesclar a PR draft e não anunciar o DustLoader como completo até fechar todos os bloqueadores.** Os pipelines permitidos geram apenas objetos nativos temporários e DLLs gerenciadas. A checagem de UI/Android não foi executada nesta etapa.
