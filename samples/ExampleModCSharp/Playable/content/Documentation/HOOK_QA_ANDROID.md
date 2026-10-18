# Hook QA CSharp 0.1.47 — alvos reais no Terraria Android

Requer APK Bunny Loader CSharp .85 (Terraria 1.4.5.8.6, arm64-v8a). Os metodos abaixo foram encontrados pelo scanner nativo IL2CPP no aparelho em 17/10/2026. Estar presente nao e o mesmo que ter callback validado.

| Hook | Candidatos em ordem, todos com assinatura real conforme scanner |
| --- | --- |
| H1 int() | Utilities.UnifiedRandom.Next (callback ja validado anteriormente) |
| H2 void(int) | WorldItem.UpdateItem e alternativas anteriores (callback OK2 anteriormente) |
| H3 void(int,int) | Wiring.HitSwitch, Wiring.HitWireSingle, Wiring.PokeLogicGate, todos static |
| H4 int(int,int,int) | Player.CalculateFrameFromMiscCounter (instancia), WorldGen.countWires (static) |
| H5 void(int,int,int) | WorldGen.Check1x1, WorldGen.CheckLogicTiles, WorldGen.Check2x2 (static), WorldItem.SpawnShimmeredItem (instancia) |

Cada modulo tenta o proximo candidato somente se o anterior falhar na instalacao. Cada callback executa original.Invoke() uma vez, nao muda parametros ou resultados, e se desinstala apos duas chamadas.

**Teste em mundo descartavel**: instale somente ExampleMod CSharp 0.1.47 sobre 0.1.46 com a mesma APK .85. Ande e anime o personagem (H4), ative alavanca ou switch (H3), coloque/quebre um bloco comum (H5). Nao tente forcar Shimmer nem eventos de worldgen. Saia para o menu e envie os cinco estados da secao hooks universais CSharp e o FPS; pare se surgir travamento.

No relatorio, ARMADO@metodo confirma instalacao, OK1/OK2@metodo confirma callbacks. OnEnterWorld mostra texto precomputado e nunca deve ser usado para verificar contadores. H3-H5 ainda aguardam teste real. Ao terminar restaure ExampleMod CSharp 0.1.42 fora do QA.
