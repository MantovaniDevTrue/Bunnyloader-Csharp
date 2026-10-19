# Pré-auditoria paralela — equipamentos C#

**Base:** o lote `loaders-02` do planejamento de 50 sistemas. Este trabalho é independente do grupo World Loaders (PR #98); não altera Tile, Wall, Tree, TileEntity, GlobalTile, MonoVM, Roslyn, SDK, runtime nativo, pacotes ou APKs.

## O que já pode ser reutilizado

| Loader | Exemplo original escolhido | Base C# já presente | Principal auditoria pendente |
| --- | --- | --- | --- |
| ItemLoader | `ExampleGun.js` | `ExampleGun.cs`, `BunnyItem` e `BunnyWeapon` | Ciclo de uso, holdout, disparo e hooks específicos do item. Uma arma compilada não demonstra todo ItemLoader. |
| EquipLoader | `ExampleBeard.js` | `ExampleBeard.cs`, `BunnyItemBehavior.EquipTexture` | Slots, camadas, textura funcional/vaidade, loadouts e restauração das tabelas nativas. |
| ArmorSetLoader | `ExampleHelmet.js`, `ExampleBreastplate.js`, `ExampleLeggings.js` | Três peças C#, `BunnyArmorSet` e policies nativas | Bônus somente com conjunto completo, imunidades, texto localizado, efeitos de frame, sombras, cabelo e desenho. |
| WingLoader | `ExampleWings.js` | `ExampleWings.cs` e `SetWingStats` | Velocidade vertical/horizontal, atualização da asa, gravidade invertida, referência por slot e animações. |
| PrefixLoader | `ExamplePrefix.js`, `ExampleAccessoryPrefix.js` | `ExampleMightyPrefix.cs`, `ExampleGuardedPrefix.cs`, `BunnyPrefix` | RollChance, CanRoll, escolha por categoria, efeitos aplicados, valores, tooltips e persistência da reforja. |

A referência dos exemplos é **o ExampleModBunny original** e a árvore de fonte do Bunny. Não usar tModLoader ou ExMod para copiar implementações nesse runtime. O documento apenas identifica contratos a revisar, não afirma que todos estão ausentes. API C# pública, policy compilada e callback efetivo de gameplay são três coisas diferentes.

## Verificador reproduzível

A partir da raiz de `Split/Bunny-Loader`:

```bash
python3 tools/equipment_preflight.py
python3 tools/equipment_preflight.py --json --output /tmp/equipment-preflight.json
python3 -m unittest discover -s tests -p equipment_preflight_test.py -v
```

O verificador lê `docs/csharp-loader-batches.json`, os cinco sistemas do grupo 2, todos os módulos JS atribuídos no plano, os oito arquivos dos exemplos originais, as oito fontes C# correspondentes e a referência pública compilada do SDK. Registra SHA-256 de cada fonte e confirma que os símbolos indicados existem nos exemplos JS. Não altera nem reescreve nenhum desses arquivos. Uma fonte ausente causa falha explícita.

**Limite importante:** localizar um nome de método em JS, uma classe em C# ou um arquivo na pasta não comprova equivalência funcional. A auditoria semântica hook a hook, os testes nativos com QuickJS desabilitado, os testes do exemplo e a validação Android ainda são obrigatórios antes de chamar qualquer um dos cinco loaders de completo.

## Testes futuros por categoria

1. **Item:** uso contínuo, munição, som próprio, disparo com direção sincronizada com a mira antes do tiro, coleta e clone vanilla; testar item por item e regredir com armas já existentes.
2. **Equip:** sprite vestido, sprite de vaidade, slots de back/shield/shoes, morte, respawn, troca de loadout, entrar/sair do mundo, saves e iluminação.
3. **ArmorSet:** três peças completas, peças misturadas, buffs imunizados, bônus numéricos sem duplicação, tela de inventário, texto e visual no mundo.
4. **Wing:** subida, queda, hover, gravidade invertida, mudanças de frame e velocidade horizontal; nenhuma chamada por quadro ao MonoVM se a policy nativa resolve o efeito.
5. **Prefix:** rolagem, reforja, preço, dano/defesa, categorias, tooltip, gravação no item e reentrada no mundo.

Todos os efeitos originais devem permanecer. Medir FPS/GC e reconhecer separadamente: *fonte presente*, *SDK compilado*, *contrato nativo instalado*, *teste host passou* e *validado no Android*. Nunca inferir 60 FPS do resultado do CI.

## Integração posterior

Este branch não altera o gate de publicação dos cinco loaders. Ao concluir o grupo World, aplicar somente a ferramenta e o documento de pré-auditoria, conferir hashes novamente e iniciar o grupo Equipamentos sem refazer a investigação dos arquivos-base. Nenhum loader fica marcado como completo automaticamente.
