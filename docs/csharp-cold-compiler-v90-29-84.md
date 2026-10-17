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
