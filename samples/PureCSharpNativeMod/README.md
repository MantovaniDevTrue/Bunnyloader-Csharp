# Pure C# Native Mod 0.2.0

Este exemplo é um mod escrito e executado em **C# NativeAOT**, sem arquivos JavaScript no pacote. Utiliza `entry: "@native"` e a exportação `bunny_mod_register_native_content` para registrar diretamente os conteúdos por meio da Host API C++.

## Conteúdo

- **Native Pure Wand** e **Native Pure Bolt**: arma e projétil teleguiado já validados na 0.1.10.8
- **Native Pure Slime**: NPC registrado diretamente pelo C++, com movimento de duas velocidades calculado no C# (`NativePureSlimeAI`)
- **Native Pure Amulet**: acessório que adiciona 4 de defesa
- **Native Pure Defense**: buff que adiciona 8 de defesa enquanto ativo
- **Native Pure Potion**: poção consumível que aplica o buff por 3600 ticks

Os sprites reaproveitam o ExampleModBunny. Não existe `.js` no mod: nem escrito, nem gerado.

## Testar no Android

Instalar o Bunny Loader C# Test **0.1.10.9** e importar o **PureCSharpNativeMod 0.2.0**. Por segurança, testar inicialmente sem outros mods, em singleplayer.

Abrir o menu de itens do mod e conferir a wand, o amuleto e a poção. Equipar o amuleto; conferir +4 de defesa. Consumir a poção; conferir +8 de defesa adicional e o ícone do buff.

No editor/console interno (somente para facilitar testes, não é JavaScript do pacote), executar:

```js
JSON.stringify(bl.csharp.externalMods(), null, 2)
```

Esperado: `nativeContent: true`, `nativeItems: 3`, `nativeProjectiles: 1`, `nativeNpcs: 1`, `nativeAccessories: 1`, `nativeBuffs: 1`, `nativePotions: 1`, `npcBindings: 1`. O resultado também indica `nativeNpcType`, `nativeAccessoryType`, `nativeBuffType` para facilitar a verificação.

O NPC pode ser invocado pela função de teste do Terraria, usando o valor real de `nativeNpcType` obtido no diagnóstico, por exemplo:

```js
Terraria.NPC.SpawnOnPlayer(Terraria.Main.myPlayer, bl.csharp.externalMods()[0].nativeNpcType, 0, 0, 0, 0)
```

A chamada de console é opcional e só serve para invocar o NPC; o mod propriamente dito não carrega scripts.

## Cautelas

O suporte C# continua **experimental**. A chamada nativa de `NPC.SetDefaults` verifica o tamanho da estrutura `NPCSpawnParams` antes de instalar o hook; se a ABI não corresponder à forma por referência, o registro do NPC é recusado em segurança, e o console de erros mostrará o motivo. Ainda precisamos validar visualmente no Android a animação, o buff, o acessório e os 60 FPS.

O sistema de mods JavaScript continua disponível sem conversão obrigatória. O novo hook C++ só aplica modificações a tipos registrados como conteúdo nativo C#; mods JS seguem pelo mecanismo anterior.

Multiplayer para as IAs C# ainda não é suportado.
