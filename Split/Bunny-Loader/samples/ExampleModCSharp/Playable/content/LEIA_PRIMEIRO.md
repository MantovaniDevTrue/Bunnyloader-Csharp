# Continuacao 0.1.38 — ExampleWings e referencia 1.9.0

Fonte editavel integrado ao ExampleMod: `Content/Items/Accessories/ExampleWings.cs`.
Crie a **ExampleWorkbench C#**, leve um **ExampleItem** e fabrique **ExampleWings**.
As asas usam a folha original, slot Wings e 180 ticks de voo; teste decolagem,
subida/queda, gravidade invertida, troca de loadout, morte/respawn e save/reload.
Teste junto das tres armaduras feitas na mesma ExampleWorkbench. Os testes
Android deste grupo continuam pendentes; um teste de bancada vanilla nao os substitui.
A implementacao atual de equipamentos ainda passa por policies JS do host;
a amostra nao exige que o autor escreva JS, mas essa dependencia interna continua
registrada como trabalho pendente. Nao chamar o Lote A de concluido.

# ExampleMod 0.1.38 / Bunny .90.29.11 — experimental

A bancada ExampleWorkbench e suas tres armaduras agora integram este
ExampleMod CSharp, sem mod separado. Consulte Content/Tiles/Furniture,
Content/Items/Placeable/Furniture e Content/Items/Armor.
A bancada e os efeitos da armadura ainda requerem verificacao no Android.
O cache C# de compilacao foi ajustado para evitar recompilacao repetida
apos falhas na aplicacao de hooks nativos.

# ExampleMod 0.1.36 / Bunny .90.29.3 — historico

Esta atualizacao documenta receitas sem intermediacao da VM JavaScript e
atualiza a matriz para 201 arquivos. Os 51 fontes e IDs anteriores permanecem.
Leia Documentation/RECEITAS_NATIVAS.md; as receitas existentes demonstram
a API publica. A bancada C# propria e os exemplos restantes nao foram incluidos.
Nao tratar esta entrega como Lote A completo nem validacao nova no Android.

# ExampleMod 0.1.35 / Bunny .90.28.5

Seis exemplos originais novos usam BunnyItem.Behavior. Leia
Documentation/LOTE2_ITENS.md para a API, testes e pendencias.
Os exemplos anteriores e seus identificadores foram preservados.

# ExampleMod 0.1.34 — itens e receitas / Bunny .90.28.3

O Bunny .90.28.3 corrige o fluxo de interpretacao V2 de BunnyItem: depois
de registrar ExampleItem/ExamplePickaxe/ExampleHamaxe, a linha nao deve cair
na validacao de PROJECTILE/WEAPON. O limite antigo de 16 foi ampliado com contratos na .90.28.5.
A versao .90.28.2 ainda rejeita ExampleHamaxe mesmo com 20 campos corretos.
Os fontes e assets do .bmod 0.1.34 continuam os mesmos.

Comece por Content/Items/ExampleItem.cs e Content/Items/Tools/ExamplePickaxe.cs /
ExampleHamaxe.cs. Os comentarios explicam cada atributo e a receita original.
BunnyItem fornece defaults reais; o host aplica atributos no Item nativo, usa o
LegacySoundStyle de SoundID e deixa o Terraria executar mineracao/melee/animacao.
SetDefaults pode calcular os atributos em C#. DisplayName e Texture precisam
ser strings literais em SetDefaults para o registro antecipado dos recursos.
Esta API nao promete OnUse/AI/callbacks de item ainda nao expostos no SDK.

AddRecipes declara receitas com CreateRecipe/AddIngredient/AddTile/Register.
A bridge instala ate oito receitas por passagem na thread do Terraria, atualiza
os lookups de crafting/materiais e so depois confirma o indice. Falhas nao viram
sucesso: veja csharp recipe + UID/classe/indice no relatorio. Mods somente de
itens nao precisam de uma arma/projetil nem de recompilacao externa.

Poderes: Pick=220; Axe=30 interno equivale a 150%; Hammer=100. UseSound="Item1"
e um som builtin. Value=50000 corresponde ao sellPrice de uma moeda de ouro.
Recipes: 10 DirtBlocks + WorkBench -> 999 ExampleItems; 10 ExampleItems +
WorkBench -> uma picareta ou um hamaxe. Confira consumo, som, mineracao/melee,
Save/Exit/reentrada, bau, loadouts, morte/respawn e convivencia com itens JS.

Documentation/COMPATIBILIDADE.json/.md contabiliza os 192 JS; Documentation/
TESTAR_ANDROID.md orienta o lote. O item ExampleItem e sua receita existem; a
moeda customizada do JS continua pendente. Host/compilacao nao provam gameplay
ou FPS no aparelho. O alvo e 60 FPS e a medicao continua no roteiro Android.

---

# Equipamentos nativos — ExampleMod 0.1.33 / Bunny .90.27.6

Guardian Shield: equipe em um slot funcional (ou de vaidade) e deixe o visual
visivel. O Terraria recebe shieldSlot e seleciona os quadros de andar/pular,
com espelhamento ao virar. A defesa e os callbacks ficam no BunnyPlayer.
Trailblazer Boots: a folha Shoes veste os pes; o rastro usa a rotina nativa de
chamas no tile sob o jogador. Corra no chao, com velocidade horizontal > 1
pixel/tick, sem montaria. Parado/pulando/morto/visual oculto nao produz chamas.
O tile ainda precisa aceitar fogo; o retorno nativo pode ser falso.
Mystic Ward: a barreira azul continua no renderer de overlay existente.

Em caso de falha, procure no relatorio:
- csharp equip: item/tipo/slot: confirma o vinculo nativo.
- texture_missing: inclui a textura ausente; confira nome, caminho e maiusculas.
- native_slot_rejected: o loader recusou aquele slot, e pode tentar novamente.
- csharp boots: contrato nativo incompleto: mostra metodos/campos indisponiveis.
- csharp boots: rotina nativa tile=..., chama=...: primeiras tentativas no chao.

Nao precisa ligar um mod JS para instalar estes equipamentos. Os comentarios
nos acessorios explicam quais partes sao textura, desenho nativo e gameplay.
Esta correcao tem testes de host; animacao/FPS no Android precisam de confirmacao.

---

# ExampleMod C# — lote 0.1.26

Requer Bunny Loader .90.25 ou mais recente. Este pacote inclui os exemplos
existentes, tres acessorios compostos e oito receitas de combate em Common/Recipes.
As receitas compilam junto do mod, mas so atuam quando chamadas por um callback.
Nao instalam oito efeitos adicionais nem alteram o balanceamento automaticamente.

## Acessorios e seus componentes

- Seis acessorios basicos preservados: Focus, Swift, Momentum, Arcane, Shadow, Vital.
- ExampleGuardianShield: +5 defesa, +4 regeneracao de vida, -15% dano de NPC e -10% de projetil.
- ExampleTrailblazerBoots: +12% velocidade, +8% aceleracao, efeito visual do FrameEffects.
- ExampleMysticWard: +3 defesa, +8 regeneracao de mana, -20% dano quando a vida esta em 35% ou menos, -10% adicional de projetil proximo (ate 192 pixels).

Cada exemplo composto tem um BunnyAccessory em Content/Items/Accessories e um BunnyPlayer em Common/Players. Os efeitos so sao habilitados quando equipados; retirando o acessorio, o bonus deve sumir. As sprites reutilizam a textura de Focus nesta fase e devem ganhar artes proprias futuramente.

## Caminho de estudo

1. Content/Items/Weapons + Content/Projectiles: ligar uma arma ao projetil com nameof.
2. Content/Items/Accessories + Common/Players: item, Tooltip e efeito ao equipar.
3. Content/Items/Accessories/ExampleGuardianShieldAccessory + Common/Players/ExampleGuardianShieldPlayer: escudo com multiplos callbacks.
4. ExampleTrailblazerBoots e ExampleMysticWard: velocidade, regeneracao e defesa condicional.
5. Content/Prefixes: multiplicadores e bonus de defesa do prefixo.
6. Common/Players/ExampleVitalPlayer.cs: ordem de ModifyHurt, FreeDodge e OnHurt.
7. Common/Recipes/01..08: reducao, vida baixa, NPC, projetil, distancia, velocidade,
   arco e observacao depois do golpe. Cada arquivo explica uso, unidades e limites.

## Integrar uma receita

Dentro do seu override ModifyHurt(bool accessoryEquipped, BunnyHurtModifiers modifiers):

```csharp
// Nao aplique o efeito so por ter o item no inventario.
if (!accessoryEquipped || modifiers.PvP || modifiers.Damage <= 0)
    return;
// Escolha UMA receita para comecar. Chamar varias acumula as reducoes.
modifiers.Damage = ExampleModCSharp.Recipes.LowLifeGuard.Apply(modifiers);
```

Nao copie outro BunnyPlayer com o mesmo nome/Accessory. Classes BunnyPlayer
instaladas atuam automaticamente; classes static Recipes so atuam quando chamadas.
ResetEffects nao deve zerar contadores persistentes de golpes a cada callback.

## Contexto e unidades

- NpcIndex/ProjectileIndex: slot temporario; -1 significa ausente. Zero e valido.
- NpcType/ProjectileType: tipo da entidade, nao seu slot.
- AttackerDistance: pixels; -1 significa sem contexto espacial.
- AttackerVelocityX/Y: pixels por tick; sinal negativo e uma direcao valida.
- AttackerAngle: radianos, eixo X positivo; nao e a orientacao do jogador.
- Damage em ModifyHurt e alteravel; dados de posicao/vida nao sao comandos.
- BunnyHurtInfo em OnHurt e um snapshot: mudar variaveis locais nao cura nem causa dano.
- Multiplayer e APIs gerais do tModLoader nao sao prometidos por este lote.

## Entender erros

Compilacao: content/arquivo.cs:linha:coluna [CSxxxx], mensagem original, trecho,
marcador ^ e orientacao quando conhecida. O relatorio limita a 60 erros e informa
quantos foram omitidos. Corrija o primeiro erro antes dos derivados.

Execucao de BunnyPlayer Hurt: BUNNY_CSHARP_CALLBACK_ERROR identifica mod, classe,
callback, tipo da excecao e mensagem. O log limita repeticoes por mod/fase.
Nao inventamos linha de runtime: sem PDB ela pode nao estar disponivel.
Essa instrumentacao cobre Register/Modify/After do dispatcher Hurt; nao todos
os sistemas de callbacks do loader.

Limites: 512 arquivos .cs, 256 KiB por arquivo, 16 MiB no total no compilador;
os limites de conteudo por categoria continuam os existentes. Nao acrescente
arquivos de teste com erros em content/: eles impedem o mod inteiro de compilar.

## Validacao do lote

Os testes de host executam todas as oito receitas, casos de fronteira, erros
reais do Roslyn e excecoes intencionais de callbacks. O relatorio JSON associa
cada caso ao resultado/erro. Isso nao substitui validacao de gameplay no Android.
Os fontes comentados deste diretorio sao os mesmos empacotados no .bmod.


