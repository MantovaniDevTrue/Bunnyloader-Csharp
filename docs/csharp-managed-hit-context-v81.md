# Bunny Loader C# .81 — plano técnico para contexto de evento gerenciado

**Status: bridge V2 experimental implementada; CI de host aprovado (#38175418299). Build Android .81 em preparação, validação no aparelho pendente.** Este documento pertence à branch experimental `csharp-managed-event-context-v81-experiment`. A fonte de regressão é a branch `csharp-android-validated-v80`, apontada ao commit de APK `ae787455f3f7adada861d211809cf8bf60c99438`; não modificá-la.

## Implementação inicial já concluída nesta branch

- `ManagedLiveQueuedEvent` recebeu **`npcIndex=-1`** como último campo, mantendo compatíveis todos os inicializadores antigos de 5 argumentos.
- Teste nativo de fila `Split/Bunny-Loader/tests/test_managed_hit_context_v81.cpp`: verifica valores do índice na FIFO64 e na ingress queue de ambos os mods; eventos antigos continuam com `-1`, hit válido conserva seu slot, pausa elimina eventos pendentes e invalida tickets antigos, reentrada lógica aceita ticket novo, kill não recebe um NPC inventado.
- [CI de regressão v81 #38174850848](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38174850848) **success** (compilação C++ host e confirmação estática de que o MonoVM ainda resolve `OnHitNPC(int,int)` e `OnKill(int,int)`).
- Implementado no código desta branch: `hkStatusNPC` encaminha o slot validado, o ingresso FIFO64 guarda `npcIndex`, o worker o repassa à resolução MonoVM e o método `OnHitNPCV2(int,int,int)` é procurado opcionalmente, com cache por classe/mod e fallback para `OnHitNPC(int,int)`. `OnKill(int,int)` segue inalterado e usa `npcIndex=-1`.
- Um exemplo opt-in V2 foi adicionado ao `ExampleWaveBoltLiveEvents.cs` (ExampleModCSharp 0.1.10); o método legado permanece presente. `BunnyLiveEvents` dos dois mods continua usando a assinatura antiga para verificar compatibilidade.
- Teste host atualizado para validar o roteamento legacy/V2, sentinela e faixa de slots. O novo workflow da branch deverá comprovar a execução em CI; **não equivale a teste em Android.**
- A APK .80 validada continua inalterada. Não publicar APK .81 até verificar CI Android e realizar testes no dispositivo.

## Problema confirmado nos fontes da .80

O SDK `Split/Bunny-Loader/experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs` expõe callbacks declarativos do projétil; os callbacks realmente executados no worker de eventos do MonoVM são `public static int OnHitNPC(int call, int damage)` e `OnKill(int call, int damage)` nas classes de evento, como `ExampleWaveBoltLiveEvents.cs`.

- O argumento `call` é uma sequência de admissão do callback, **não** índice/ID do NPC.
- `ManagedLiveQueuedEvent` guarda `tick`, `damage`, `isKill`, `enqueuedAtNs` e `bindingSlot`; não guarda identidade do alvo.
- `ManagedHitHook.cpp::hkStatusNPC(projectile, index, method)` recebe o índice do NPC e valida `index` dentro de [0, 200), `Main.npc`, alvo ativo e suas condições antes do enfileiramento da execução gerenciada.
- `ManagedKillHook.cpp::hkKill(projectile, method)` **não** recebe o índice de um NPC: evento de morte do projétil não deve receber um `npcIndex` inventado.
- `ManagedRuntime.cpp::invokeLiveSourceEvent` resolve métodos `OnHitNPC`/`OnKill` com dois argumentos `int`, via `g_api.findMethod`/ `g_api.invoke`, sob contrato atualmente testado no Android.

A documentação e o conteúdo de ExampleModBunny.zip são a referência da organização dos mods JS, **não** fonte de APIs tModLoader para copiar. O trabalho do C# deve partir das assinaturas e hooks já implementados no Bunny, com paridade de comportamento gradualmente verificada.

## Design de implementação V2 — experimental

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

1. Implementado: fila com `npcIndex`, teste da FIFO64, ingresso com duas bindings e proteção de pausa/reentrada.
2. Implementado: hook nativo transmite `index` validado somente para hits; kills preservam `-1`.
3. Implementado: descoberta cacheada de `OnHitNPCV2(int,int,int)` por classe, sem afetar a chamada antiga `OnHitNPC(int,int)`. Quando `npcIndex=-1`, usa apenas o callback legado.
4. Implementado: exemplo opt-in V2 e smoke nativo de seleção. CI de host aprovado; pendentes compilação Android ARM64 e testes de integração no aparelho.
5. Pendente: APK experimental `.81` após CI e revisão; validar 2 mods/3 armas, 100 NPCs Blood Moon, 60 FPS, reentrada, pausa/retomada e contadores.

**Limitações assumidas:** a primeira evolução é um contexto primitivo de *hit*; contexto tipado com identidade estável de NPC, efeitos no mundo, NPC AI por frame e multiplayer são etapas independentes e **não** são implementados aqui. Qualquer sugestão de suporte a mods genéricos do PC exige evidências específicas.


## Correção de regressão do mock host

O primeiro build Android falhou nos testes de host porque `managed_pinned_gc_mock.cpp` exigia `params == 2` para todos os métodos. A `.81` agora procura opcionalmente `OnHitNPCV2` com aridade 3. O mock passou a verificar cada nome com sua aridade correta e o teste `managed_hit_v2_mock.cpp` exerce despachos antigos, V2, `OnKill`, slots inválidos e pausa. A compilação ARM64 e a validação Android continuam pendentes até conclusão da próxima execução do workflow.
