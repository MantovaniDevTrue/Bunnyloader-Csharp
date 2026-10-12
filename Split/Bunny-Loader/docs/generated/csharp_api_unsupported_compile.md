# Bunny C# — verificacao negativa de APIs ainda ausentes

Cada caso compila intencionalmente uma chamada C# **nao suportada**.
O teste so passa quando Roslyn a rejeita com o diagnostico esperado.
Compilacao negativa nao comprova ausencia de recursos na API JavaScript ou no Terraria.

Total: 5/5 rejeicoes corretas.

| API em pesquisa | Erro Roslyn esperado | Resultado |
| --- | --- | --- |
| item_prefixes | CS0246 | pass |
| item_recipes | CS1061 | pass |
| npc_loot | CS1061 | pass |
| projectile_collisions | CS1061 | pass |
| player_dash | CS1061 | pass |
