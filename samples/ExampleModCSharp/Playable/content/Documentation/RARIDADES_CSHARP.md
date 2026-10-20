# Raridades C# nativas — ExampleModBunny

As classes em `Content/Rarities` portam os dois exemplos originais do Bunny Loader JavaScript usando somente o SDK C#.

| Classe | Cor | Prefixo |
| --- | --- | --- |
| `ExampleModRarity` | RGB (200, 215, 230) | Prefixo com aumento de valor sobe para `ExampleHigherTierModRarity` |
| `ExampleHigherTierModRarity` | RGB `DiscoR/2, DiscoG/1.25, DiscoB/1.5` | Prefixo que diminui valor desce para `ExampleModRarity` |

Referências reais no pacote: `ExampleItem` e `ExampleMeleeWeapon` demonstram a raridade base; `ExampleStatAccessory` demonstra a superior. `ExampleWand` usa o registro tipado em `BunnyWeapon`; `ExampleBuffPotion` demonstra o mesmo em `BunnyBuffPotion`. Texturas, efeitos e receitas permanecem nas classes originais.

## QA Android pendente

Com o mod ativado, observar a cor em inventário, chão, coleta, tooltip, chat e depois salvar/reabrir. Aplicar prefixos bons e ruins na espada/acessório, confirmar que o ID e a cor correspondem ao tier após cada alteração. Testar o ciclo com raridade JS e C# juntos, inversão da ordem dos mods e com o mod desabilitado/habilitado. Fazer a primeira conversão de save somente em mundo descartável; conferir o backup `.before-rarity-v3.bak`, baús, cofres e equipamento depois da restauração.

Os hooks podem recusar instalação se a assinatura IL2CPP do Terraria Android não corresponder à verificação nativa. Os testes de host não garantem fidelidade visual ou FPS no aparelho. Não usar a versão em desenvolvimento com saves de jogatina.
