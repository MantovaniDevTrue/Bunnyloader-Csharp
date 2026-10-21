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

O `AutoSourceRuntime::run()` coleta módulos C# que possuem `BunnyWorldSystem`, inclui esses módulos no prewarm do MonoVM, e envia pedidos `Register` ao worker pinado depois da compilação Roslyn. Se qualquer módulo falhar, guarda o erro de diagnóstico por mod e impede `preWorldCSharpContentReady()` de liberar a entrada no mundo: nenhum mundo entra em execução supondo incorretamente que suas flags foram carregadas.

Isso completa a **descoberta e o registro do módulo no worker**, ainda sem execução de `Enter/Prepare/Confirm/Leave` ligada aos hooks reais de salvamento e carregamento.

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
