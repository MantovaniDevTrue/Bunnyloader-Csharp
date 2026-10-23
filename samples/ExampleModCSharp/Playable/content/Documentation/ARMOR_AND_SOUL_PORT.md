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

Os textos en-US/pt-BR das peças preservam as traduções originais. Desde a
versão fonte 0.1.69, o conjunto usa `UseLocalizedText(ArmorSetLocalizationKey)`.
`ExampleLocalizationModule.RegisterTranslations` lê todas as traduções de
`ArmorSetBonus.ExampleArmor` do JSON original e substitui `{0}` pela mesma
constante 20 usada nos efeitos. A chave formatada é publicada no catálogo do
próprio UID: não é necessário fixar o UID no código do item.

O protocolo opcional A2 acrescenta uma chave relativa; o A1 antigo continua
aceito. O loader passa `Mods.<UID>.<chave>` a ArmorSetBonuses.Add, que guarda
um LocalizedText em Description no Android 301720. Ele aguarda Language.Exists
antes de registrar, para não capturar um LocalizedText provisório fora do
catálogo. O publisher de localização atualiza esse texto ao trocar de idioma;
a ausência de tradução usa o fallback en-US do catálogo, sem duplicar conjuntos.
Isso mantém a localização e os quatro bônus de dano independentes.

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
ABI, iluminação, cor e tabelas. As 24 variantes de armadura incluem o protocolo
real localizado, espera pela chave e sete falhas de acesso a Language.Exists.
São testes de host; aparência, idioma do bônus no aparelho, gameplay, rede e
execução Android permanecem pendentes. Nenhum APK é gerado.

Na versão fonte 0.1.70, desativar ou recarregar o mod retira imediatamente
seus efeitos das consultas de equipamento. A limpeza de All, SetsContaining
e dos handles de GC acontece na thread do jogo. Ela remove apenas registros
do conjunto retirado, conserva outros conjuntos e pode continuar após uma
falha parcial. Até terminar, o delegate emprestado do conjunto Pumpkin fica
suprimido para os IDs retirados: não pode conceder um bônus vanilla por engano.

Um snapshot completo sem itens também retira as políticas anteriores. IDs
nativos podem permanecer reservados; o loader exige que as peças ainda façam
parte do snapshot ativo. Isso também desativa um conjunto de outro mod que
dependa de uma peça retirada, sem afetar os conjuntos independentes. Os testes
exercitam ambos os protocolos A1/A2, metadados crus/boxed, falhas de limpeza,
100 recargas antes do próximo frame e recuperação de Add que alterou All
antes de lançar uma exceção. O registro é reencontrado por IDs e delegate,
sem depender de um índice antigo na lista.
