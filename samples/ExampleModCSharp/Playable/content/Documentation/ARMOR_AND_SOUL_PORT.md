# Armaduras e alma do ExampleMod Bunny

Os fontes ficam em `Content/Items/Armor` e `Content/Items/Materials`, com as
classes e imagens originais no mesmo mod. A referência deste lote é o Bunny JS
1.9.0 anexado; não é uma versão substituta simplificada do ExampleMod oficial.

Nas três peças, `SellPrice(gold: 1)` preserva o valor interno **50.000** de
`Terraria.Item.sellPrice(0, 1, 0, 0)`. `BuyPrice(gold: 1)` retorna **10.000**:
trocar os helpers muda o preço. As peças continuam 18 × 18, raridade Green,
defesa 5/6/5, e cada receita usa um ExampleItem na ExampleWorkbench.

O peitoral usa as mesmas constantes no texto e nos efeitos: +20 de mana,
+1 lacaio e imunidade a OnFire. As calças acrescentam 0,05 a moveSpeed.
O conjunto completo acrescenta 0,20 a cada multiplicador melee, ranged, magic
e minion; peças parciais não ativam esse bônus. O loader de armaduras aplica
os efeitos em cada atualização de equipamento, como no exemplo JS.

Os textos en-US/pt-BR das peças preservam as traduções originais. A descrição
do bônus de conjunto ainda usa inglês: falta localizar a entrada nativa de
ArmorSetBonuses durante a troca de idioma. Essa pendência mantém AddArmorSets
fora dos comportamentos considerados equivalentes neste lote.

ExampleSoul clona SoulofSight, usa Pink, BuyPrice(gold: 1), pilha 9999 e
animação 4/6. As três tabelas de alma são nativas. PostUpdate ilumina o centro
do item com `(0.53, 0.81, 0.92) * 0.45 * Main.essScale`; GetAlpha devolve
RGBA `(255, 255, 255, 50)`. Não há JavaScript para esses efeitos.

Strings interpoladas de C# são executadas pelo Roslyn. A descoberta antecipada
apenas as percorre como expressões opacas, incluindo interpolações aninhadas,
comentários e strings verbatim interpoladas. DisplayName/Texture continuam
exigindo literais para reservar recursos antes da execução do mod. Isso não
é uma implementação completa da gramática C# nem uma avaliação das expressões
pelo parser nativo.

`bash tests/armor_port_run.sh` compila os fontes reais, compara valores e
tooltips com o JSON original, executa o parser e os loaders C++ de produção
com o protocolo emitido pelo Roslyn. Cobre peças/conjunto, recarga, falhas de
ABI, iluminação, cor e tabelas. São testes de host; aparência, idioma do bônus,
gameplay, rede e execução Android permanecem pendentes. Nenhum APK é gerado.
