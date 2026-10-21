> Histórico: para a .95, use SUMMON_LOADER_V95.md; as instruções e limitações abaixo descrevem a entrega anterior.

# Receitas próprias + projéteis configuráveis — ExampleMod 0.1.52

Esta versão reúne o loader de receitas .92 e a infraestrutura minion da build
.90.5. Não instale o NativeMinionCSharpProbe junto para este roteiro: o exemplo
agora está neste pacote, com classes e assets em arquivos separados.

## Arquivos para estudar

- `Content/Projectiles/ExampleMinion.cs`: quatro frames, cadência nativa,
  intervalo/alcance da busca e `AI(ref BunnyProjectileAiState)` real no MonoVM.
- `Content/Buffs/ExampleMinionBuff.cs`: vínculo por nome; renovação enquanto
  houver o projétil e remoção quando não houver, sem gravar o buff no personagem.
- `Content/Items/Weapons/Summon/ExampleMinionItem.cs`: cajado, vínculo com buff e
  projétil e receita usando o mesmo loader público dos demais itens.

`AnimationFrameTicks` (1..120), `MinionScanTicks` (1..120) e
`MinionTargetRange` (1..4000 pixels) são propriedades de BunnyProjectile, lidas
uma vez por instância pelo núcleo. O motor anima sem chamar MonoVM por frame e
varre NPCs na cadência declarada. O último alvo fica em cache entre varreduras;
reduzir MinionScanTicks aumenta o custo de CPU. Não há garantia de 60 FPS.

`BunnyMinionMovement` permite configurar velocidade de ataque/retorno,
inércia, posição de repouso, distância de recuperação e amortecimento. Crie a
configuração uma vez, como no campo estático do exemplo. O overload antigo de
`BunnyMinionSteering.FollowOrAttack(ref state)` continua disponível. O helper
altera velocidade; ele não teleporta nem escreve entidades diretamente.
Você também pode implementar sua própria AI usando o mesmo snapshot.

## Teste no Terraria Android

1. Atualize APK e ExampleMod juntos. Faça backup e use um mundo de teste.
2. Coloque **ExampleWorkbench C#**. Com ExampleItem no inventário, fabrique
   **Example Minion Staff** (consome um ExampleItem). Afaste-se para confirmar
   que a receita depende da bancada C#.
3. Use o cajado: confira textura e quatro quadros animados do minion, buff ativo,
   acompanhamento e dano contra NPC hostil. Mova-se, mate o alvo e procure outro.
4. Remova o buff e observe se ele é retirado quando não houver mais projétil.
   Registre se o minion persiste indevidamente: o ciclo de remoção ainda exige
   validação; não interprete apenas o ícone como prova de funcionamento completo.
5. Teste morte/respawn, sair/reentrar e nova invocação. Não deve restar callback
   de uma instância anterior nem buff salvo sem projétil.
6. Teste também as três receitas da .92: Example Wand, Focus Accessory e Defense
   Potion. Cada uma gasta dez ExampleItem e concede um após fabricar; recolha a
   recompensa caso apareça junto ao jogador. Guia: OWNED_RECIPE_LOADER_V92.md.
7. Altere AnimationFrameTicks de5 para10, MinionTargetRange de700 para300 e
   MinionScanTicks de8 para16 no arquivo C#, reimporte o pacote e compare o
   comportamento. As fontes são compiladas automaticamente; não precisa DLL.
8. Repita com Lights And Shadows e o seu mod JS habilitado. Guarde relatório,
   nome do conteúdo, momento da falha, versão e FPS observado.

## O que permanece parcial

O exemplo não é ainda um port integral do JS: faltam teleporte, separação entre
minions, alvo manual, linha de visão, luz e rotação original. O cajado ainda não
expõe todos os atributos originais (mana, som, valor, raridade, knockback, spawn
na mira); a receita adicional usa a bancada C#. O buff não demonstra o contador
nativo de projéteis no texto. A matriz conserva essas pendências explicitamente.
As APIs novas funcionam sobre as bridges reais, mas testes host não comprovam
ABI/gameplay Android. O Lote A e todos os loaders não estão concluídos.
