# Summons C# — .95 / ExampleMod 0.1.54

Teste somente com o **ExampleMod CSharp**, junto dos outros mods que você já usa.
Desative o antigo NativeMinionProbe para evitar confundir os conteúdos.

1. Pegue ExampleItem e ExampleWorkbench no catálogo de mods. Coloque a bancada C# e fabrique ExampleMinionItem com 1 ExampleItem.
2. Use o cajado. Deve aparecer ExampleMinion e seu buff na barra de poções/buffs.
3. Remova **esse buff**. O minion deve desaparecer imediatamente; ele não pode recolocar o buff sozinho. Repita com várias invocações: todas do mesmo dono devem terminar.
4. Invoque novamente. Ande, corra, pule e vire. O minion deve acompanhar, manter formação e retornar depois do combate; longe demais, deve teleportar até você.
5. Aproxime-se de um inimigo. Ele deve procurar um NPC atacável, perseguir, causar dano e voltar. Um alvo bloqueado por parede fora dos 100 pixels não deve ser escolhido; fora do limite do dono, deve abandonar a perseguição.
6. Morra e renasça. A invocação antiga não pode reaparecer sozinha. Invoque novamente e confira o funcionamento.
7. Salve/saia e reentre. Confira cajado/bancada/receita preservados e ausência de invocações antigas.

O relatório deve registrar `Stage=Lifetime Result=Killed Reason=BuffCancelled` ao cancelar e `Stage=ApplyStatsFromSource OriginalDamage=... IncomingDamage=... ItemDamage=...` nos primeiros disparos. Não espere spam por frame.

## API reutilizável

`BunnyProjectile.MinionMovement = new BunnyMinionMovement(...)` seleciona um controlador **nativo**, atualizado a cada tick. As propriedades de movimento são públicas e independem do nome do exemplo. `FollowOffsetX` segue a direção do dono; `FormationSpacing` separa os companheiros do mesmo tipo/dono; `OwnerLeashDistance` limita perseguições; `TeleportDistance` recupera invocações distantes. `AiStyle` deve ser 0.

Para implementar sua própria IA em C#, mantenha `MinionMovement = null` e sobrescreva `AI(ref BunnyProjectileAiState)`. O compilador recusa combinar os dois controladores, em vez de ignorar o código. Callbacks C# usam snapshots e a cadência assíncrona existente; eventos de spawn/acerto/morte continuam ligados ao MonoVM. O ciclo de vida por buff/dono permanece nativo nos dois casos.

`BunnyBuff.MinionProjectile` associa o buff ao projétil; `BunnyWeapon.Summon`, `Shoot` e `MinionBuff` associam o cajado. Use nomes de conteúdo, sem IDs numéricos persistidos. O loader inicializa `originalDamage` a partir do item real na origem do disparo, preserva o dano escalado recebido e deixa o Terraria recalcular o dano da invocação. Não escolhe o maior dano entre armas registradas.

Esta entrega corrige o sistema; não certifica todo o ProjectileLoader. Continuam pendentes os atributos completos do cajado, spawn na mira, luz original, contador textual do buff e outros exemplos. Os testes host executam hooks reais com objetos IL2CPP controlados; comportamento, ABI e FPS no Android ainda precisam desta validação. Não há garantia nova de 60 FPS.
