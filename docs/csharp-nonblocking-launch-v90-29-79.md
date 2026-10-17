# C# .90.29.79 — preparação fora da interface

Base preservada: `codex/csharp-prefetch-reuse-v90-29-77`, commit
`8aa6e1ceacf650c9921ae9ff20565813b7d5e1b1`. ExampleMod 0.1.40 permanece
inalterado. Esta branch não substitui o experimento .78 de leitura de tiles.

## Evidência e alteração

Na .77, `LauncherActivity` chama `LaunchSourcePrecompile.begin` antes de
`startActivity`. `begin` abria o manifesto do framework, consultava arquivos de
cache e percorria todos os `.cs` na thread da interface. O processo isolado de
Roslyn não eliminava esse trabalho síncrono.

Agora `begin` somente captura até 16 especificações e inicia o serviço enquanto
o launcher está em primeiro plano. Uma tarefa na fila do `BunnyMonoCompiler`
inspeciona os pacotes com prioridade reduzida. Não se inicia um serviço Android
tardiamente a partir de uma thread do launcher já em segundo plano. Uma lista
vazia não inicia o serviço. Fontes continuam sendo validadas no worker e no
compilador; nenhum SetDefaults é executado pelo prefetch.

Pedidos oficiais mantêm prioridade na fila; removem o prefetch pendente do mesmo
mod. A inspeção também evita enfileirar prefetch para um mod com pedido já
pendente. O gate de conteúdo, restauração, bindings e timeout antes do mundo
permanece inalterado. Não se entra em um mundo com registros essenciais incompletos.

`CSharpPrefetchReceipt` passa a um arquivo Kotlin sem dependências Android,
utilizado diretamente pelo serviço. Seus limites antigos (512 arquivos,
256 KiB/arquivo, 8 MiB total) impediam reaproveitamento para fontes aceitas pelo
compilador atual. Agora correspondem a 4.096 arquivos, 1 MiB e 32 MiB; a listagem
é interrompida no primeiro arquivo acima do limite. O formato SHA-256 V1 é
preservado. Reutiliza-se um buffer de leitura e a conversão hexadecimal deixa de
usar String.format por byte. Não foi removida verificação de integridade.

## Testes e limites da evidência

- `tests/test_csharp_launch_precompile.py`: contratos de integração e limites,
  sem atribuir a essas asserções uma execução Android.
- `CSharpPrefetchReceiptTest`: execução JVM da implementação usada pelo serviço;
  recibo válido, edição com timestamp preservado, adição/remoção de fonte,
  mudança de framework, DLL/cache adulterados, fonte editada durante a compilação,
  4.096/4.097 fontes, limites de bytes, escape por symlink e contrato V1.
- Pipeline executa `:app:testDebugUnitTest` e `:app:assembleDebug`, seguidos das
  verificações existentes de assinatura e publicação experimental privada.

Não há medição Android nova nesta alteração. Os testes de recibo não medem
Roslyn/Mono no Xiaomi nem comprovam a política de serviços do Android. A menor
latência de primeira compilação ainda depende do custo real de framework,
Roslyn, defaults e registro nativo; esta alteração remove uma fonte de bloqueio
da interface e amplia o reaproveitamento seguro.

## Validar com o ExampleMod no Terraria

1. Usar o ExampleMod CSharp 0.1.40 da build, sem substituir por item vanilla de
   teste. Preservar saves e registrar APK, pacote e quantidade de mods.
2. Após uma mudança de fonte do ExampleMod (comentário basta), clicar Jogar e
   observar se a interface responde durante a abertura. Não apagar dados/saves.
3. Entrar no mundo e verificar bancada, receitas de armadura, ferramentas,
   acessórios e projéteis do ExampleMod. Salvar/sair e entrar novamente.
4. Reabrir sem editar fontes. Comparar `CompileMs`, `WaitTablesMs`,
   `FrameworkMs`, `RoslynInvokeMs`, `VerifyInvokeMs` e `PrefetchReused`.
5. No relatório, `source-launch-startup.txt` inclui `queueWaitMs` e `scanThread`.
   A varredura deve identificar `BunnyMonoCompiler`, nunca a thread principal.
   `elapsedMs` mede a inspeção do worker; não é o tempo do clique nem da
   compilação. Um hit do cache nativo pode pular o worker oficial, sem emitir
   `PrefetchReused=1`; isso não significa falha do cache.
6. Registrar tempo do clique até menu/mundo, maior pausa percebida, RAM e FPS no
   mesmo aparelho. Nenhuma promessa de 60 FPS ou de redução percentual é feita
   sem esses resultados.

Pendentes: testes Android cold/warm e com múltiplos mods; medição separada de
registro/rehidratação na thread do jogo; análise das próximas etapas guiada pelos
contadores, preservando consistência de conteúdo e saves.
