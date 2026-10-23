# WorldSaveLoader C# — fundação de fonte (sem APK)

Base canônica: Split/Bunny-Loader/samples/ExampleMod/content/Common/Systems/DownedBossSystem.js e app/src/main/cpp/script/js/mod/Loaders/SystemLoader.js. Nenhum código do tModLoader foi utilizado.

## Entregue nesta etapa

- BunnyWorldSystem com callbacks ClearWorld, OnWorldLoad, LoadWorldData, PostWorldLoad, SaveWorldData, OnWorldUnload.
- BunnyWorldTag tipada (bool, int32 e string UTF-8), campos ausentes seguros, limites por chave e tipo.
- BunnyWorldDocument com identidade estável UUID/classe, isolamento entre mods, cópias defensivas, leitura determinística, limites de 1 MiB/512 sistemas/128 campos, CRC32 e recusa de corrupção, truncamento, duplicações e bytes extras.
- Exemplo comentado ExampleDownedBossWorldSystem; testes reais de codec em .NET 8; workflow exclusivo de compilação de código-fonte.
- Nenhum APK, .bmod, asset proprietário, pacote de release ou integração QuickJS.

## Etapa 2 — sidecar nativo

O NativeWorldStore agora tem implementação compilável para leitura e escrita de `.wld.csharp.bwc`, sem modificar o `.wld`. Exige caminho absoluto fornecido pelo host, rejeita nuvem, travessia de diretório e links simbólicos no arquivo; usa openat(O_NOFOLLOW), tamanhos limitados, CRC32 e validação de campos do BWC1. Serializa operações por sessão e por epoch, preserva o arquivo prévio se inválido, grava em arquivo temporário com fsync + rename e força sincronização do diretório. Testes C++ verificam corrupção, sessões, multi-mundo, symlink e concorrência.

O serviço está **deliberadamente desligado**: o caminho real do mundo e os callbacks gerenciados não estão vinculados ao IL2CPP/MonoVM. A compilação nativa de host prova apenas o componente POSIX, não o hook Android. A estrutura deste módulo fica isolada do QuickJS.

## Etapa 3 — ciclo de vida nativo (sem ativar persistência gerenciada)

O hook já existente em `ModTileSave.cpp` passa o caminho local vindo de `ActiveWorldFileData` para `noteAutoWorldTilesLoaded`, na mesma ordem da restauração das tiles. O `AutoSourceRuntime` inicia o `NativeWorldStore` usando o `g_worldEpoch` atualizado; ao detectar saída do mundo, invalida a sessão anterior. Nuvem, diretório inacessível e caminhos inválidos falham de forma fechada. A referência global ao armazenamento só existe em C++ e não cria uma API para o mod definir caminhos.

Este passo **não registra** os callbacks `BunnyWorldSystem`, não lê/escreve dados via Mono e não afirma que os hooks de save do Terraria estão integrados. O teste do workflow verifica os pontos de conexão em fonte; assinatura e offsets reais ainda exigem QA Android.

## Etapa 4 — dispatcher gerenciado de sistemas de mundo

O novo `BunnyWorldDispatcher` descobre e registra sistemas por UUID/classe, rejeita duplicações e alterações de registro após entrada, e executa `ClearWorld → OnWorldLoad → LoadWorldData → PostWorldLoad`. `TryPrepareSave` cria o snapshot sem mutar o último documento confirmado, e `TryConfirmSaved` só deve ser chamado após sucesso do sidecar nativo. Uma exceção bloqueia o snapshot inteiro; `TryLeaveWorld` invalida o epoch e os estados. Clientes não podem persistir no disco. Mod ausente mantém dados originais.

Estes são métodos executáveis no SDK; **a chamada entre o MonoVM e os hooks reais ainda não está implementada**. O teste .NET usa as próprias classes do SDK e o ExampleDownedBossWorldSystem de exemplo.

## Prova de interoperabilidade

A CI gera um `BWC1` diretamente com `BunnyWorldDocument.Encode()` em .NET 8 e passa o arquivo para o teste C++ POSIX. Os bytes devem coincidir exatamente com o documento nativo `document(42)`, e o validador C++ precisa aceitar o payload. O fixture temporário não é publicado nem instalado.

## Etapa 5 — fachada para o MonoVM

`BunnyWorldMonoHost` encapsula `RegisterMod`, `Enter`, `Prepare`, `Confirm`, `Leave` e `ResetRegistrations`. O `RegisterMod` usa a mesma descoberta por `Assembly.LoadFrom` do `BunnyEntitySourceDispatcher` já existente. Base64 limita-se às operações raras de mundo, com tamanho máximo fixo e rejeição de payload inválido. A fachada nunca grava arquivo, não oferece path a mods e só confirma snapshot quando o host mandar. Registros tardios e épocas antigas são recusados. Teste adicional compila uma DLL de mod C# independente e a carrega dinamicamente para validar o caminho real de descoberta/execução.

O código pode agora ser invocado pelo worker MonoVM, mas o **dispatcher nativo de pedidos para este worker ainda falta**: nenhuma função IL2CPP ou save real é chamada por esta fachada isolada.

## Etapa 6 — fila limitada para o MonoVM

O `MonoWorldMailbox` agora define os pedidos tipados de registro, entrada, preparação, confirmação e saída do mundo. Limite de 8 pedidos, tamanho máximo de payload, prazo de resposta, invalidação por geração e descarte de callbacks tardios. O teste com sanitizadores cobre cancelamento, fila cheia, timeout e troca de mundo.

A fila ainda não foi ligada à execução do worker fixo; **não há hooks de save ativos nem APK**.

## Etapa 7 — mailbox integrada ao worker MonoVM

O loop existente de `pinnedMonoWorker` lê agora a fila `MonoWorldMailbox` com prioridade antes dos jobs de hit/entidades. A execução ocorre no mesmo worker dedicado, sob o mutex de API Mono, abrindo `BunnySourceSdk.dll` e chamando `BunnyWorldMonoHost.RegisterMod / Enter / Prepare / Confirm / Leave` por nomes e parâmetros tipados reais. Strings retornadas são copiadas por `mono_string_to_utf8` e liberadas por `mono_free`. Os pedidos são limitados, têm prazo de até cinco segundos, não atravessam ponteiros IL2CPP, e são cancelados nas mudanças do `g_worldEpoch`.

O host já expõe `requestManagedWorldOnMono` para os futuros pontos de save/load. **Ainda não há chamada a essa função pelos hooks de salvamento do Terraria**: a conexão com o jogo depende de validar a ABI Android e as regras de gravação/autoridade.

## Etapa 8 — admissão de mod C# contendo apenas BunnyWorldSystem

O `SourceParser` e `SourceLoader` agora reconhecem `BunnyWorldSystem` como tipo gerenciado, sem exigir `SetDefaults` ou um item artificial. Cada classe é registrada pelo namespace completo, sem identificador numérico do Terraria. A verificação Roslyn `CompiledAssemblyProbe` permite assemblies que tenham exclusivamente sistemas de mundo, exige construtor público vazio e aplica limite de 512 sistemas. A CI inclui teste de parser C++ sanitizado.

O registro efetivo no MonoVM depende de agendar `MonoWorldAction::Register` com o caminho compilado e a ativação em `Enter`; não considerar isso concluído ainda.

## Etapa 9 — registro efetivo de sistemas compilados

O `AutoSourceRuntime::run()` coleta módulos C# que possuem `BunnyWorldSystem`, inclui esses módulos no prewarm do MonoVM, e envia pedidos `Register` ao worker pinado depois da compilação Roslyn. Se qualquer módulo falhar na compilação ou no registro, mantém o gate fechado, guarda o erro de diagnóstico por mod e impede `preWorldCSharpContentReady()` de liberar a entrada no mundo: nenhum mundo entra em execução supondo incorretamente que suas flags foram carregadas.

Isso completa a **descoberta e o registro do módulo no worker**, ainda sem execução de `Enter/Prepare/Confirm/Leave` ligada aos hooks reais de salvamento e carregamento.

## Etapa 10 — transação C# / nativo, com validação do arquivo

O novo NativeWorldCoordinator chama a fila tipada no worker MonoVM: Enter lê o BWC1 existente (ou indica mundo novo), Prepare captura um snapshot sem confirmar, o validador rejeita Base64 não-canônico/BWC1 inválido e NativeWorldStore escreve com fsync/rename. Só depois disso ocorre Confirm. Leave invalida o sidecar da sessão. Saves de clientes, sessões antigas e dados corrompidos falham fechados. Testes C++ de produção, com worker gerenciado simulado e fixture BWC1 gerada pelo SDK C#, exercitam as fases e asseguram que o arquivo original do Terraria não é modificado.

Esta transação ainda precisa ser chamada por hooks reais de LoadWorld e InternalSaveWorld; nenhuma gravação de jogo foi ativada neste ponto.

## Etapa 11 — hooks nativos estritos

A integração de Terraria.IO.WorldFile.LoadWorld(bool) chama Enter apenas após o carregamento original. O hook InternalSaveWorld(bool, bool) aguarda o salvamento original e então executa Prepare → BWC1 → sidecar atômico → Confirm. O epoch carregado também é verificado para bloquear gravações antigas, nuvem e clientes. No caminho de saída, Leave é chamado antes de invalidar o sidecar da sessão. A instalação requer assinatura exata por nome, tipo e nome de parâmetros, e método estático confirmado pela API IL2CPP. Caso a assinatura não exista, os sistemas de mundo não devem liberar a entrada.

A saída via WorldGen.SaveAndQuitCallBack(object) também chama OnWorldUnload no MonoVM e invalida o sidecar após o callback original, com limpeza adicional pelo monitor do menu para casos sem callback. O teste de sintaxe C++ no Linux ainda não valida a ABI efetiva do Terraria Android; isso requer QA nativo. Também falta suporte real de rede para o multiplayer.

## Etapa 12 — formato gerenciado de rede BWN1

BunnyWorldSystem agora inclui NetSend(BinaryWriter) e NetReceive(BinaryReader), usando apenas System.IO do C# nativo. ExampleDownedBossWorldSystem demonstra a flag de progresso enviada pelo servidor e lida pelo cliente, como o ExampleModBunny. BunnyWorldNetworkCodec serializa o envelope BWN1 com revisão, UUID/classe, payload por sistema, limite de 60000 bytes e checksum CRC32. Valida integralmente antes de entregar bytes aos callbacks. Os testes .NET exercitam roundtrip, corrupção, truncamento e pacote excedente. **Ainda não há transporte de rede instalado no Terraria**; não anunciar multiplayer como pronto.

## Etapa 14 — transporte nativo BWN1 isolado

`NativeWorldNetwork` valida integralmente o envelope BWN1 antes de entregá-lo ao MonoVM (magic, versão, contador, ordenação, UUID, nomes, tamanho por sistema, CRC32). Usa `MonoWorldAction::NetSend/NetReceive` na thread dedicada, limita o payload a 60 KB e reserva o id de módulo 0xB17F, distinto do módulo JavaScript 0xB17E. A fixture binária vem da implementação C# real, e o teste C++ compara byte-a-byte e verifica corrupções. Este componente **não instala sozinho NetManager.Read / SendData**; esses hooks dependem da ABI IL2CPP nativa e devem ser checados antes de qualquer envio de jogo.

## Etapa 15 — interface IL2CPP de transporte da rede

`NativeWorldNetTransport` instala hooks exatos de `NetManager.Read(BinaryReader,int,int)` e `NetMessage.SendData(...)` (mensagem 7), utilizando `NetPacket(ushort,int)`, `BinaryWriter.Write(byte[])` e `SendToClient/Broadcast`. Módulos desconhecidos têm sua posição original restaurada e passam ao jogo; somente id `0xB17F` é consumido. No cliente, pacotes de servidor entram em `onNativeWorldNetReceived`. No servidor, após a mensagem 7, chama-se `onNativeWorldSyncSent`. A resolução falha fechada quando a API é incompatível, sem substituir o tráfego `0xB17E` de mods JavaScript.

Estes callbacks de sessão ainda precisam ser ligados ao `AutoSourceRuntime` para sincronização real; o compilador host valida sintaxe, mas não substitui o teste de ABI no Android.

## Etapa 17 — guarda de desempenho na rede

Os dois hooks de NetManager verificam `nativeWorldNetActive()` antes de inspecionar o pacote ou de executar a chamada C#. Quando nenhum mod possui BunnyWorldSystem, os pacotes continuam pelo callback original sem reflexão IL2CPP adicional. Para um sistema ativo, só a mensagem 7 (servidor) dispara NetSend; o cliente apenas recebe BWN1 da conexão com o servidor. Gravação nativa em mundo local também é autorizada no netMode 2 (servidor).

O caminho quente não tem varreduras por entidade. A validação em dispositivo Android continua obrigatória.

## Etapa 18 — identidade estável de mods reais

`SourceModStatus.id` deriva do UID de pasta (por exemplo, `examplemodcsharp`) e **não é obrigatoriamente um UUID**. Antes de chamar o `BunnyWorldMonoHost.RegisterMod`, o host converte UIDs de pasta com SHA-256 namespaceado em um UUID determinístico (versão 8, variante RFC), sem depender da ordem de carga, dispositivo, endereços nativos ou índice do Terraria. UIDs que já são UUIDs válidos preservam sua forma canônica. A identidade original da pasta continua sendo a utilizada pelo loader, pelos assets e pela compilação C#. O teste valida repetibilidade, separação entre mods, canonicidade e entradas inválidas.

Esta conversão não foi aplicada retroativamente a saves antigos porque os arquivos BWC1 ainda não foram lançados ao público. Não altera UUIDs declarados por mods com UID já canônico.

## Requisitos ainda bloqueantes

1. Host nativo C++ deve capturar a identidade real do arquivo de mundo após a seleção; não confiar em nome de mundo ou caminho informado por um mod.
2. Instalar hooks IL2CPP corretos de ClearWorld, LoadWorld, InternalSaveWorld e SaveAndQuit, preservando o original e a ordem observada no SystemLoader.js. Confirmar offsets e assinatura no Terraria Android 1.4.5.8.6.
3. Estabelecer transporte limitado entre thread do jogo e worker MonoVM, com epoch de sessão, timeout, fila de salvamento e proteção contra trocas de mundo. Falhas de callback nunca devem corromper o arquivo existente.
4. Gravar sidecar C# com troca atômica e sync (temporário no mesmo diretório + rename); validar sandbox de caminho, saves locais/cloud e indisponibilidade de disco. Preservar dados de mods não instalados e não sobrescrever sidecar se leitura falhar.
5. Descobrir e registrar sistemas C# automaticamente via compilador e ciclo de vida, com isolamento de exceções, unload e detecção de identidades duplicadas.
6. Definir NetSend/NetReceive e autoridade de servidor para sessão multiplayer, com testes host/cliente/entrada tardia; sem integração de rede, não anunciar paridade do exemplo JS.
7. Testar no aparelho: criar mundo A, matar boss, salvar/sair, carregar, ir a mundo B, voltar, desligar o mod, troca de mundo, save corrompido, dois mods, sessões longas e FPS. Repetir após recompilar Roslyn e limpar cache.

**Gate:** o WorldSaveLoader NÃO está completo e NÃO pode habilitar a API em produção nem gerar APK/.bmod. O codec é apenas a primeira peça executável e testável. Nunca promover compilação de host como validação de hooks do jogo.

## Como testar sem APK

dotnet build Split/Bunny-Loader/experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj -c Release

dotnet run --project Split/Bunny-Loader/tests/world_save/BunnyWorldData.Smoke.csproj -c Release

O workflow check-csharp-worldsave-source executa esses dois comandos apenas na branch dedicada.


### Etapa 20 — isolamento da rede assíncrona e rollback

- `WorldNetAsync` limita a 32 trabalhos por sessão e prepara `NetSend` e executa `NetReceive` fora da thread de renderização. A função `noteAutoGameUpdate` envia no máximo quatro resultados por frame, usando exclusivamente a thread do Terraria e conferindo epoch/autoridade. Na troca de mundo, a geração do worker é invalidada e resultados atrasados são descartados.
- A rotina de rede não espera os três segundos do MonoVM dentro de `NetManager.Read` nem de `NetMessage.SendData`; o timeout subsiste em worker e nos raros hooks transacionais de persistência.
- `BunnyWorldDispatcher` desfaz uma entrada mal sucedida e permite sessão posterior mesmo se `ClearWorld`, `OnWorldLoad`, `LoadWorldData` ou `PostWorldLoad` falhar; nenhum sidecar corrompido pode ser salvo.
- `ManagedRuntime` agora aplica o limite correto de 80 KB Base64 a retornos `NetSend`, não o limite de 1,4 MB dos saves.
- A CI exercita cancelamento por geração, fila limitada, callbacks e retorno ao mundo, além de compilação estrita C++/C# host-only. Continua pendente QA Android/ABI real.


### Etapa 21 — ABI Android (compilação apenas)

O job android-arm64-source-only valida os módulos nativos com NDK v30, produzindo apenas objetos ELF AArch64, sem APK ou .bmod. A compilação não comprova link final ou ABI Terraria em jogo. O cliente normaliza a origem do NetManager.Read para 256, como o ExampleModBunny, mantendo o gate netMode=1.
