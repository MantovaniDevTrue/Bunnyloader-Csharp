# Bunny Loader C# .81 — plano técnico para contexto de evento gerenciado

**Status: base de transporte e teste de compatibilidade implementados com CI aprovado; callback C# V2 ainda NÃO implementado nem testado no Android.** Este documento pertence à branch experimental `csharp-managed-event-context-v81-experiment`. A fonte de regressão é a branch `csharp-android-validated-v80`, apontada ao commit de APK `ae787455f3f7adada861d211809cf8bf60c99438`; não modificá-la.

## Implementação inicial já concluída nesta branch

- `ManagedLiveQueuedEvent` recebeu **`npcIndex=-1`** como último campo, mantendo compatíveis todos os inicializadores antigos de 5 argumentos.
- Teste nativo de fila `Split/Bunny-Loader/tests/test_managed_hit_context_v81.cpp`: verifica valores do índice na FIFO64 e na ingress queue de ambos os mods; eventos antigos continuam com `-1`, hit válido conserva seu slot, pausa elimina eventos pendentes e invalida tickets antigos, reentrada lógica aceita ticket novo, kill não recebe um NPC inventado.
- [CI de regressão v81 #38174850848](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38174850848) **success** (compilação C++ host e confirmação estática de que o MonoVM ainda resolve `OnHitNPC(int,int)` e `OnKill(int,int)`).
- **NÃO IMPLEMENTADO:** o hook nativo ainda não preenche o novo campo, o worker ainda não o encaminha e nenhuma classe C# possui novo método de hit ativo. A APK .80 validada continua igual. Não publicar APK .81 até realizar essas etapas e obter CI Android.

## Problema confirmado nos fontes da .80

O SDK `Split/Bunny-Loader/experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs` expõe callbacks declarativos do projétil; os callbacks realmente executados no worker de eventos do MonoVM são `public static int OnHitNPC(int call, int damage)` e `OnKill(int call, int damage)` nas classes de evento, como `ExampleWaveBoltLiveEvents.cs`.

- O argumento `call` é uma sequência de admissão do callback, **não** índice/ID do NPC.
- `ManagedLiveQueuedEvent` guarda `tick`, `damage`, `isKill`, `enqueuedAtNs` e `bindingSlot`; não guarda identidade do alvo.
- `ManagedHitHook.cpp::hkStatusNPC(projectile, index, method)` recebe o índice do NPC e valida `index` dentro de [0, 200), `Main.npc`, alvo ativo e suas condições antes do enfileiramento da execução gerenciada.
- `ManagedKillHook.cpp::hkKill(projectile, method)` **não** recebe o índice de um NPC: evento de morte do projétil não deve receber um `npcIndex` inventado.
- `ManagedRuntime.cpp::invokeLiveSourceEvent` resolve métodos `OnHitNPC`/`OnKill` com dois argumentos `int`, via `g_api.findMethod`/ `g_api.invoke`, sob contrato atualmente testado no Android.

A documentação e o conteúdo de ExampleModBunny.zip são a referência da organização dos mods JS, **não** fonte de APIs tModLoader para copiar. O trabalho do C# deve partir das assinaturas e hooks já implementados no Bunny, com paridade de comportamento gradualmente verificada.

## Design escolhido para a próxima implementação — revisão obrigatória

Adicionar um contrato **opt-in e versionado** exclusivamente para hits com o alvo identificado: `OnHitNPCV2(int call, int damage, int npcIndex)` em uma classe `*LiveEvents`. Um callback que só declare a assinatura antiga continuará operando exatamente como na .80. `OnKill(int call, int damage)` permanece inalterado neste estágio.

A identidade entregue é um **snapshot primitivo** do slot `npcIndex` validado no instante do hit, **não** um ponteiro, nem um objeto Terraria, nem garantia de que o mesmo NPC ainda ocupa esse slot quando o worker executar. O código gerenciado **não poderá usar esse índice para mutar entidades**; isso exigirá protocolo futuro com epoch/generation e comandos aplicados na thread do jogo.

### Requisitos de segurança/performance

1. Capturar e copiar o índice inteiro no hook que já conhece o NPC e já faz a validação do hit. Não expor Il2CppObject, arrays gerenciados Unity, memória nativa do jogo ou mutações diretas para a thread do worker.
2. Preservar o fluxo FIFO64, `bindingSlot`, identidades/gerações da sessão, limite 25.000, política de falhas por vínculo e pause/resume de v79.1; preservar a cobertura do profiler por projétil de v80.
3. Reutilizar `invokeLiveSourceEvent` e o cache por assembly/método existentes; resolver assinatura nova uma vez por vínculo/assembly, depois usar o cache. O antigo `OnHitNPC(int,int)` permanece fallback completo.
4. Considerar `npcIndex` válido somente para eventos `OnHitNPC`; não fabricar índice no evento `OnKill` nem usar `0` como sentinela válida.
5. Não acrescentar thread, espera ativa, lock extra nem alocação por frame/projétil. Fora dos callbacks opt-in, nenhuma mudança sem teste de regressão.
6. Comportamento da arma, mira/direção do jogador, dano, partículas, sons, buffs e curvas do ExampleModCSharp deve permanecer idêntico ao .80.

### Provas necessárias antes de build Android

- Teste host com dois mods e três armas, chamadas antigas e V2 intercaladas, retornos e números de eventos verificáveis.
- Eventos atingindo slots de NPC diferentes, índice inválido rejeitado, simulação de troca de mundo e pausa/retomada, incluindo produtor atrasado do PR #15.
- Evento `OnKill` antigo intacto, inclusive para projéteis que não atingiram NPC.
- Comparar `sourcePerfStatus().projectiles` com as métricas da .80. CI verde incluindo build C++ ARM64 e novos testes.
- Instrumentação opt-in, captura do resultado no editor, sem afirmar `FPS=60` até teste real no aparelho.
- O ExampleModCSharp deve ter uma variante V2 **somente depois que a bridge compilar**; documentação sempre separa API existente, CI verificado e Android validado.

## Plano de execução

1. Criar o teste que hoje falha porque `ManagedLiveQueuedEvent` não contém `npcIndex`, e revisar os guardas do hook atual.
2. Implementar a passagem do slot de NPC para os eventos de hit, preservando `OnKill` e as APIs antigas.
3. Adicionar descoberta/caching de `OnHitNPCV2(int,int,int)` com fallback seguro para `OnHitNPC(int,int)`.
4. Criar exemplo V2 e testes de integração na estrutura real do ExampleModCSharp.
5. Preparar APK `.81` somente após CI e revisão de regressão; testar 2 mods/3 armas, 100 NPCs Blood Moon, 60 FPS, reentrada, pausa/retomada e relatar contadores.

**Limitações assumidas:** a primeira evolução é um contexto primitivo de *hit*; contexto tipado com identidade estável de NPC, efeitos no mundo, NPC AI por frame e multiplayer são etapas independentes e **não** são implementados aqui. Qualquer sugestão de suporte a mods genéricos do PC exige evidências específicas.

