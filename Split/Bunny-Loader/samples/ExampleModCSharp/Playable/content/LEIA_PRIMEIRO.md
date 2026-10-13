# ExampleMod C# — lote 0.1.25

Requer Bunny Loader .90.25 ou mais recente. Este pacote inclui os exemplos
existentes e oito receitas de combate em Common/Recipes, numeradas para estudo.
As receitas compilam junto do mod, mas so atuam quando chamadas por um callback.
Nao instalam oito efeitos adicionais nem alteram o balanceamento automaticamente.

## Caminho de estudo

1. Content/Items/Weapons + Content/Projectiles: ligar uma arma ao projetil com nameof.
2. Content/Items/Accessories + Common/Players: item, Tooltip e efeito ao equipar.
3. Content/Prefixes: multiplicadores e bonus de defesa do prefixo.
4. Common/Players/ExampleVitalPlayer.cs: ordem de ModifyHurt, FreeDodge e OnHurt.
5. Common/Recipes/01..08: reducao, vida baixa, NPC, projetil, distancia, velocidade,
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

Limites: 64 arquivos .cs, 256 KiB por arquivo, 2 MiB no total no compilador;
os limites de conteudo por categoria continuam os existentes. Nao acrescente
arquivos de teste com erros em content/: eles impedem o mod inteiro de compilar.

## Validacao do lote

Os testes de host executam todas as oito receitas, casos de fronteira, erros
reais do Roslyn e excecoes intencionais de callbacks. O relatorio JSON associa
cada caso ao resultado/erro. Isso nao substitui validacao de gameplay no Android.
Os fontes comentados deste diretorio sao os mesmos empacotados no .bmod.
