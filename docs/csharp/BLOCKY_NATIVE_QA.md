# Blocky / ExampleCostume — C# nativo

Estado: implementado na branch de QA; compilação integral e gameplay Android ainda não validados. Não aumentar a contagem de comportamentos validados no Android.

## Fonte

Referência prioritária: `ExampleModBunny(1).zip` fornecido pelo portador. Conteúdo fonte equivalente em `samples/ExampleMod/content/Common/Players/ExampleCostumePlayer.js`, `Content/Buffs/Blocky.js` e `Content/Items/Armor/ExampleCostume.js`. Há uma divergência no efeito do traje seco: o ZIP original usa `GoldFlame` e a amostra atual no repositório usa `Sparkle`. Usar o ZIP no port da partícula.

## Implementação

- `BunnyItemBehavior.ConfigureCostume` declara conjunto alternativo, buff, salto, queda e requisito de NPCs. Parser nativo valida o descritor.
- `NativeItemPolicies` instala Head/Body/Legs normais e alternativos, grava flags nativas de ocultação de pele com restauração condicionada ao descarregamento e publica uma política imutável.
- `NativeContent` reutiliza os hooks de equipamento, UpdateBuffs, Update e SetArmorEffectVisuals com estado por jogador. Resolvem-se campos e assinaturas do Terraria Android por contrato; em caso de incompatibilidade a política de traje fica pendente, sem escrever em offsets arbitrários.
- O caminho comum não consulta inventários nem texturas de traje quando nenhuma política está ativa.
- `ExampleCostume.cs` ativa o comportamento C# sem executar JavaScript.

## Testes necessários

1. Compilar projeto C#, testes de parsing e biblioteca Android/arm64.
2. Entrar no mundo com um traje real, sem crash e sem novas mensagens de erro de contrato.
3. Conferir com e sem NPC da cidade: buff de 60 ticks, +4.8 de salto, +45 de queda, remoção ao afastar-se ou desequipar.
4. Conferir slots funcionais e de vaidade; ocultar visual mantém efeito funcional, apenas vaidade não concede buff.
5. Conferir texturas Head/Body/Legs, skin flags e alternância normal/molhado em multiplayer.
6. Testar recarregamento do mod, troca de mundo/loadout e remoção de buffs sem restos de estado.
7. Medir FPS, memória e travamentos num aparelho real. Não existem medições desta revisão.

## Pendências

- Partículas originais `GoldFlame`/ `BlueFlare`, probabilidade 1/20, ainda não integradas.
- Confirmar ordem real de `Player.Update` / `UpdateBuffs` / `SetArmorEffectVisuals` no Android. Paridade dos callbacks não deve ser presumida antes do teste.
- O `ExampleCostumePlayer.cs` ainda é uma implementação de regras Playable; os hooks nativos executam a política serializada do item, não chamadas arbitrárias ao objeto gerenciado por quadro.
- Não gerar APK de lançamento até o restante da paridade do ExampleMod C# estar concluída.
