# .84 — compilador frio: meta de cinco segundos ainda pendente

Base: `.83`, commit `e21c12b338ae59d8f811efd7fb1a0818269acbfc`, com ExampleMod
0.1.44 e todas as correções de H1–H5 preservadas. Não depende de mesclar a antiga
PR .79 nem modifica hooks, IDs, execução de gameplay ou o gate do mundo.

## Evidência do Xiaomi, 17/10/2026

- Roslyn da primeira pré-compilação: **12.114 ms**.
- Compilação gerenciada + preparação dessa chamada: **12.704 ms**.
- Reaproveitamento oficial: `RoslynInvokeMs=0`, `VerifyInvokeMs=588`,
  `FrameworkMs=9`, `WorkerTotalMs=966`.
- Identidade de cache: **315 ms**; aplicação dos defaults: **5 ms**.
- `CompileMs=11232` inclui a espera pela pré-compilação ainda em andamento; não
  deve ser somado a `RoslynInvokeMs=12114` como se fossem duas compilações.

Logo, a espera reportada é real e está principalmente no compilador frio. Não
há evidência para atribuí-la ao cache nem para prometer cinco segundos só
antecipando a operação ou diminuindo o timeout do mundo.

## Implementação

1. O APK contém um conjunto separado `compiler-references/`, com as referências
   oficiais .NET 8.0.23 e a reference assembly pública gerada pelo próprio SDK
   Bunny. Roslyn não precisa importar as DLLs completas de implementação nessa
   configuração. Mono continua executando as assemblies originais do runtime.
2. A instalação verifica SHA-256 de cada arquivo, incluindo referências em
   subpasta. O download do pacote oficial de referências verifica SHA-512.
3. `CompilerReferences` lê snapshots imutáveis e mantém **um** conjunto de
   `MetadataReference` por processo. Mods diferentes reutilizam metadados;
   alterações nos bytes invalidam o conjunto. Árvores de fonte e compilações de
   mods não ficam retidas nesse cache. Sem habilitar paralelismo irrestrito ou
   aumentar a retenção de assemblies de mods.
4. O hash do compilador inclui o conteúdo das referências, e o cache nativo
   inclui as DLLs de `compiler-references`. Modificar apenas referências não
   pode reutilizar defaults antigos. Cache anterior será reconstruído pela
   nova identidade do compilador/referências; não apaga saves.
5. O perfil frio é salvo por mod e recuperado quando o worker oficial reaproveita
   uma DLL pré-compilada. `Stage=RoslynProfile` registra sourcesMs,
   fingerprintMs, parsingMs, referencesMs, emitMs, referenceMode,
   referenceBytes, metadataReused, totalMs e invocationMs. `RoslynInvokeMs`
   continua sendo a medição externa, incluindo entrada/JIT antes do método.

A seleção compacta requer o conjunto completo. Uma subpasta incompleta causa
rejeição explícita; instalações antigas sem subpasta preservam a seleção anterior
como fallback. Não se expõem assemblies de referência ao carregador Mono.

## Prioridade quando o jogo precisa do resultado

O prefetch da .83 usa nice +3 durante a compilação inteira, mesmo depois de
chegar um pedido oficial pelo mesmo mod. Agora esse pedido promove o worker
ativo à prioridade original (normal). Se o pedido chegou antes da publicação
do trabalho ativo, o prefetch já começa na prioridade normal. A compilação em
andamento não é cancelada nem reiniciada. No fim, a prioridade é restaurada e o
próximo trabalho especulativo recebe sua própria prioridade reduzida. Não há
prioridade de áudio/display nem compilação paralela adicional.

`Stage=PrefetchPromoted` evidencia a promoção. Quatro testes JVM executam o
controlador real para os dois ordenamentos, restauração, mod seguinte e erro de
prioridade recuperável. A repercussão em latência/FPS exige medição Android.

## Importância do processo compilador

Pedidos oficiais usam um vínculo transitório `BIND_IMPORTANT` do cliente do jogo
para o serviço isolado. Isso permite ao Android propagar a importância do cliente
ativo ao compilador, além do nice da thread. Não usa prioridade de áudio/display
nem serviço foreground com notificação permanente. O vínculo termina quando o
status do próprio pedido chega a finished/failed, se o serviço desconectar ou
após 180 segundos. Morte do cliente também encerra o binding pelo Android.

Cada pedido tem nonce, propagado ao status. Um resultado antigo do mesmo mod não
libera um vínculo novo. O controle dessa comparação é testado na JVM; binding,
cpuset, proteção de memória e efeito em FPS/tempo precisam de teste Android.
Sem assumir que a existência de um binding comprova cinco segundos.

## Espera de assets após o conteúdo essencial

A .83 impunha seis segundos antes de usar o caminho já existente para assets
adiados. Agora, se o núcleo pendente é zero, receitas e colocações estão
resolvidas, itens instalados e bindings Mono prontos, esse caminho pode continuar
imediatamente. Não se considera isso uma compilação concluída mais rapidamente:
é a remoção de uma espera fixa secundária. O fallback antigo de seis segundos
para as demais políticas nativas e o timeout de segurança são preservados.

`worldDependentAssetsMayDefer` é usado diretamente pelo gate e testado com núcleo,
receita, colocação, registro, binding e instalação pendentes (todos bloqueiam).
Assets continuam na fila original; requer validar aparecimento e efeitos no
Android, incluindo asas, escudo e botas do ExampleMod.

## Validação

`tests/compiler_references` compila **todos os fontes do ExampleMod atual** em
processos separados contra DLLs completas e referências compactas, executa os
defaults, compara os três protocolos de conteúdo/movimento/combate, confirma cache
de assembly e reaproveitamento de metadados em um segundo emit real, rejeita
referência ausente/corrompida e verifica recuperação. O relatório inclui tempo
frio no host, bytes e contagens reais de tipos/métodos das referências.

As métricas de host não representam Mono ARM64. AOT seletivo do Roslyn pode ser
uma próxima investigação se o perfil continuar dominado por JIT/emit; não está
implementado nem é alegado nesta entrega.

## Aceite Android obrigatório

Usar ExampleMod 0.1.44. Para causar uma compilação nova, editar um comentário em
um `.cs` sem remover saves. Medir desde a solicitação de preparo até os mods
prontos e até o controle efetivo do personagem. Repetir sem mudanças para cache.
Coletar `Stage=RoslynProfile`, `MonoWorkerTimeline`, `DefaultsRequest` e resumo de
preparo. Conferir bancada/receitas, equipamento, combate, H1–H5 e reentrada.

**Meta: primeira compilação/preparo em até 5.000 ms no aparelho de referência.
Não considerar atingida até medir no Xiaomi.** Não trocar essa validação por
apenas `PrefetchReused=1`, APK compilada ou teste de referência no host.

## Medição preliminar no host (não Android)

Run 39026676705, antes do ajuste de uma expectativa antiga do smoke da .44:
ambas as modalidades compilaram 64 fontes, executaram 43 tipos e produziram
protocolos iguais. Runtime: 1.429 ms, 5.149.240 bytes de referências, 2.832 tipos,
36.461 métodos. Compacto: 1.278 ms, 1.260.336 bytes, 1.148 tipos, 13.958 métodos.
`emitMs` foi 1.120/1.107; `fingerprintMs`, 170/41. O ganho no host concentra-se
na leitura/hash e metadados, não comprova resolver sozinho o JIT frio no Mono.
A promoção e o binding Android foram acrescentados para tratar a competição de
CPU/importância enquanto o jogo espera; ainda exigem evidência no dispositivo.

O perfil preservado também inclui `nativeWallMs` e `workerProcessCpuMs`. Este
último mede CPU do processo compilador durante a chamada nativa (inclui GC e
threads auxiliares), não CPU do jogo. A comparação entre parede e CPU ajuda a
distinguir custo real do compilador de espera/disputa de CPU no Android; CPU
pode ultrapassar parede se várias threads trabalharem simultaneamente.

## Corrida de RPC encontrada na regressão

O host reproduziu timeout de leitura em `NativeAccess` apesar do pedido estar
pendente. O produtor notificava o mailbox ainda segurando o mutex de RPC; o
pump do jogo podia falhar no try-lock, consumir o despertar como ausência de
trabalho e voltar a dormir até o prazo do hook. O estado agora é atômico: se o
pump não conseguir o lock mas houver pedido pendente, informa trabalho pendente
para o mailbox tentar novamente dentro do prazo original. O pump do jogo
continua não bloqueante. O produtor Mono, já fora do lock Mono, não retorna Busy
só por disputar com uma consulta ociosa do jogo.

O fixture agora acorda o worker por trabalho e sincroniza sua ociosidade entre
cenários; não disputa o mutex do mailbox em um loop ocioso. As asserções de
valores, canário, tipo, original, expiração e cancelamento continuam intactas.
A implementação anterior falhou com esse fixture (timeout de prefixo); a
corrigida passou em 1.000 processos consecutivos no host. CI exige 40 execuções.
Prazos de RPC/hook em produção não foram aumentados. Isto é evidência de host,
não validação de FPS ou latência Android.
