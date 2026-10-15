# Validacao da fundacao do SDK/runtime

Base original: .90.29.31, commit `7229a61`; atualizacao .90.29.32.1
de defesa nativa (`3dd8c8d`) e .90.29.33 (`13e53fe`) integradas. Alteracoes em
`codex/csharp-sdk-runtime-foundation`. Nao houve teste Android novo nesta etapa;
versionCode/versionName e o ExampleMod existente nao foram incrementados.

## Verificacoes reproduziveis

| Verificacao | O que exercita | Limite da evidencia |
| --- | --- | --- |
| `tests/sdk_foundation/SdkFoundation.csproj` | Roslyn real, heranca/atributos, abstratas, wire, fontes alteradas, DLL truncada/corrompida, referencias ausentes/alteradas e os tres samples | Runtime .NET do host, nao Mono ARM64 |
| `tests/basic_items/ItemBatch.csproj` | Conjunto existente, 56 fontes/42 tipos na base consultada, receitas, asas, armadura, bancada e 192 classes | Contratos e wire; nao mundo renderizado |
| `tests/run_csharp_foundation.py` | Parser nativo dos fontes declarados; um milhao de entradas; 32 vinculos/64 pendentes; ordem por vinculo, cotas, pausa e geracao | Fila nativa real em host |
| Mesmo runner: auto-source-cache | Vetores conhecidos SHA-256, fontes renomeados/alterados, todos os assemblies obrigatorios, referencia opcional adicionada/removida, DLL ausente/substituida, truncamento em cada byte e corrupcao de descriptor | I/O e funcoes reais de AutoSourceRuntime no host; sem medicao ARM64 |
| `run_native_equip_effects.py` | Wire de Roslyn real; efeitos do ExampleStatAccessory e StrongBees isolado do WaspNest; sample independente NativeCharm; 10 mil ciclos; tipo/offset/static/hook rejeitados; fallback misto preservado e marcador JS sem bonus duplicado | QuickJS desativado; IL2CPP e hooks simulados; visual Back do WaspNest continua JS |
| Mesmo runner: continuous-runtime | 75 mil chamadas por ManagedRuntime de producao, 32 bindings, falha do principal, continuidade do secundario e reentrada | Biblioteca Mono simulada |
| Mesmo runner: hurt-deadline | Callback bloqueado, timeout de outro ainda pendente, ausencia da chamada cancelada, descarte do retorno atrasado e retomada | VM simulada; nao interrompe codigo gerenciado ja iniciado |
| `run_managed_lifecycle_regressions.py` | Suite existente de identidades, ingress, contexto e efeitos, isolamento e memoria | Host/mock |
| `run_managed_pinned_gc.py` / `run_managed_signal_ownership.py` | Transicoes GC e preservacao de sinais nos modos existentes | Sinais/Mono simulados; ARM64 exige teste proprio |
| `test_create_csharp_mod.py` | UID, namespace, projeto, pacote deterministico, recusa de sobrescrita e ausencia de JS/DLL/SO | Criacao/empacotamento, nao importacao Android |
| `tools/sdk_reference --check` | Referencia corresponde ao SDK publico compilado | Superficie do SDK; nao equivale a cobertura de jogo |

O novo workflow `check-csharp-sdk-foundation.yml` executa os testes no SHA exato.
Os testes existentes permanecem; esta bateria nao os transforma em validacao
universal de todas as APIs. O cache externo tem envelope proprio, distinto do
manifesto do compilador. A regressao JS de acessorios reconstrui descritores
legados somente no teste para preservar cobertura do adaptador anterior; o novo
caminho de producao e exercitado separadamente com QuickJS desativado.

## Roteiro Android do checkpoint

1. Registrar APK/commit, Android/ABI, mods ligados, tempo total e cold/warm start.
2. Carregar ExampleMod existente junto de StarterMod. Conferir registro antes
   do mundo, crafting, consumo e tooltips sem mensagens normais repetitivas.
3. Com o novo SDK, importar DeclaredContent: um item concreto, nenhum item para
   a base abstrata, defaults herdados, receita, salvar e reabrir.
4. Exceder 25 mil callbacks automaticos em sessao prolongada. Registrar contagens,
   descartes, latencia, memoria RSS/heap/threads e FPS renderizado ao longo do teste.
   `eventLimit=0` deve permanecer ativo. O benchmark manual ainda termina em 25 mil.
5. Dois mods e varios projeteis: falha intencional do vinculo principal, secundario
   continua; pausar/retomar um adicional; sair/reentrar em outro mundo. Nao aplicar
   eventos, modificadores ou efeitos pendentes ao mundo seguinte.
6. Conferir defesa, velocidade, asas, armadura, prefixos, visuais, loadouts,
   inventario/bau e morte/respawn do conjunto anterior. As policies JS restantes
   devem continuar funcionando durante o desacoplamento futuro.
7. Testar dano com varios mods e capturar `hurtScheduler`: timeouts, cancelamentos,
   retornos atrasados e espera. Um timeout nao deve aplicar um retorno antigo a
   outro golpe. Nao usar media de FPS para ocultar travamentos pontuais.
8. Importar NativeAccessoryCSharp com apenas mods C#. Craftar NativeCharm,
   equipar/remover, trocar loadout, usar vaidade e repetir apos save/reabertura.
   Confirmar dano melee, velocidade, abelhas e defesa; nao conceder bonus em vaidade.
   O WaspNest ainda precisa de seu caminho visual JS.
9. Comparar cold/warm start e o custo RuntimeCacheIdentity; testar atualizacao de
   SDK/compilador, DLL faltando, cache truncado e bit alterado em descriptor.
   Recompilar e recuperar sem aplicar dados parciais. Preservar cache perante
   dependencia de jogo ainda pendente.
10. Repetir ciclos de pausa/retomada do app; observar som, estabilidade e memoria.
   Multiplayer continua fora do suporte demonstrado desta etapa.

Falhas de Android, ABI, memoria e gameplay continuam exigindo logs e correcao
proprios, mesmo quando todos os testes acima passam no host.
