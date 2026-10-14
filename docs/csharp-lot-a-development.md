# Lote A — desenvolvimento integral, sem entrega parcial

Branch: codex/lot-a-complete-v90-29, derivada do commit
28a9d71dac28a4ce14bff16fb492707c55227d05 da branch validada.
O usuario confirmou mascara/robe e carregamento na .90.28.7. Essa versao
permanece intacta. O ExampleMod continua 0.1.35 enquanto o lote estiver aberto.

O escopo obrigatorio possui 33 arquivos, todos listados em
Split/Bunny-Loader/tools/example_completion/lot-a-scope.json. A matriz global
continua com 199 arquivos. Dependencias em lotes futuros precisam de bridges
reais; nao justificar simplificacoes dos exemplos.

## Infraestrutura presente nesta branch

BunnyItem.Defense recebe 0..1000 e e persistido no SetDefaults nativo com
metadata integral de 8/16/32 bits, offset validado e escrita da largura real.
O registrador informa mod, classe, fonte, campo, tipo encontrado, valor e causa.

Behavior.Armor.Add(BunnyArmorInteger.StatManaMax2,20), Add(MaxMinions,1),
ImmuneTo("OnFire") e Add(BunnyArmorStat.MoveSpeed,0.05f) usam UpdateEquip
do ItemLoader original. Os alvos sao verificados antes de aplicar os efeitos.
Imunidades sao resolvidas uma vez por BuffID; nao buscar classes por quadro.
O Terraria faz seu reset habitual: o host nao retém bonus por jogador.

BunnyArmorSet declara as tres chaves de conteudo e o texto do conjunto; sua
Effects utiliza a mesma API. DefineArmorSet insere o conjunto em ArmorSetBonuses
original, com lookup/tooltip nativos e callback de bonus apenas no conjunto
completo. Chaves locais recebem UID do mod; chaves UID/Nome sao resolvidas
pelo registro nativo, sem nomes especiais do ExampleMod.

Nenhum arquivo ExampleHelmet/Plate/Leggings foi anunciado como concluido: os
contratos sao exercitados por um mod independente de testes (ContractHelmet,
ContractPlate, ContractLegs), que usa compilacao Roslyn e wire reais no CI.
O port final ainda exige receitas na bancada de mod, assets e outros hooks.

## Testes e continuidade

ItemBatch testa a API publica, rejeicoes e compilacao dos fontes de contrato.
armor_behavior_bridge_test.js carrega ModItem, ItemLoader, ArmorSetLoader e
CSharpArmorPolicy reais com doubles de Terraria. Verifica defesa, GrantArmorBenefits,
imunidade, mana/minions, velocidade, bonus de 20%, conjunto incompleto, vanilla,
reset/remoção e falha antes de aplicar efeitos. native_item_binding_test.cpp
verifica defesa em nova instancia, sentinelas e rejeicao de valor/tipo incorreto.
Nenhum desses testes comprova gameplay/FPS no Android.

CI compila um APK de desenvolvimento, mas o passo de publicacao no Builds
privado exclui esta branch. Nao publicar APK ou bmod novo como entrega do Lote A
antes de implementar os 33 arquivos e registrar todas as dependencias.

Proxima dependencia: contrato C# de tile/estacao e receitas com chave de tile,
para ExampleWorkbench. Depois portar integralmente o conjunto e Costume;
seguir o escopo completo para wings/boots/shield, consumiveis/pickups, tools,
raridades/prefixos, tooltips/currency/quests e dependencias de combate/pesca.
