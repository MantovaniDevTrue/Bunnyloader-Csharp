# Independencia C# — auditoria e checkpoint .90.29.1

Branch: `codex/lot-a-complete-v90-29`. Base preservada: .90.28.7;
referencia Android anterior .90.28.4. Nao e entrega completa do Lote A.

| Sistema | Classificacao | Caminho real / pendencia |
| --- | --- | --- |
| Defaults basicos de BunnyItem | API C# original com bridge propria | MonoVM → SourceLoader → NativeBasicItems; campos tipados. O refresh de instancias agora tambem compila sem QuickJS. |
| Reserva e identificacao de itens | Servico nativo independente compartilhado | ItemRegistry.cpp extraido do core, mesma ordem, chaves e reserva de itens ausentes; JS e C# continuam usando registerModItem. |
| Reserva e identificacao de tiles | Servico nativo independente compartilhado | TileRegistry.cpp extraido do core, mesma ordem e chaves; instalacao/draw/save permanecem em ModTiles/ModTileSave. Reserva de ID nao comprova tile funcional. |
| Receitas BunnyItem | Dependencia corrigida | NativeBasicItems → content/recipes/NativeRecipes → runtime_invoke de Recipe/Item. Nenhuma chamada a __blSyncCSharpRecipes. |
| Dependencias de receitas | Servico nativo independente compartilhado | modTileTypeByKey/modItemTypeByKey; UID/nome, mesma identidade para JS/C#. Resolver espera tile instalado e tenta novamente ingredientes tardios. |
| SyncCSharp legado no JS | Caminho de compatibilidade desativado no runtime novo | nativeRecipes=true impede registro duplicado. Nao e necessario para receitas novas. |
| Equipment/armor/wings policies de BunnyItem | Dependencia de QuickJS ainda existente | CSharpItemBehaviorLoader/ItemLoader/CSharpArmorPolicy/CSharpWingPolicy. O host sem QuickJS recusa estes descritores explicitamente. Nao declarar todo o SDK independente. |
| Bancada C# e itens colocaveis | Recurso pendente | Falta API gerenciada e configuracao nativa de TileObjectData, frames, suporte, drops, dust, mapa, housing, adjacencia e icone da estacao. |
| Limite ampliado de receitas | Recurso pendente de desacoplamento | Fora de Recipe.maxRecipes, o original utiliza hooks JS para menu/Guia/variantes. O servico novo recusa o indice excedente com NativeRecipeLimitReached; nao afirma que estes hooks estao portados. |
| Demais categorias | Auditoria pendente por sistema | APIs de combate/entidades possuem bridges proprias; esta auditoria nao presume independencia de todos os hooks e assets. |

## Contrato do servico de receitas

SDK publico permanece `BunnyItem.CreateRecipe`, `BunnyRecipe.AddIngredient`,
`AddTile` e `Register`; sem alteracao de assinaturas. `AddTile(int)` aceita
TileIDs vanilla 0..753. `2195` era indevidamente usado como limite: e indice da
paleta do mapa, nao TileID.Count. `AddTile(string)` usa nome local ou UID/nome,
resolvido no registro nativo, sem consultar TileLoader.ByType.

Antes do commit, o servico espera SetupRecipes e os itens/tiles instalados.
Dependencias e setup ainda pendentes geram WARN; falhas reais geram ERROR.

O descriptor e resolvido na thread de update, ate oito receitas por rodada,
com round-robin e intervalo de 120 updates. Dependencia ausente nao impede
aplicar defaults do item nem registrar receitas independentes. O diagnostico
inclui mod, classe, arquivo, ID, receita, estacao, etapa, esperado/encontrado e
causa; excecoes de invocacao preservam Exception.ToString quando disponivel.
Erros repetidos identicos nao sao logados novamente.

O adapter valida tipos de campos, static/instance, tamanho Int32, limites dos
offsets contra class_instance_size, assinatura/instance dos metodos, arrays e
layout do parametro ItemVariant. Nunca escreve stack em campo incompatível.
Recipe e criada separadamente; validacao de itens/pilhas precede o commit.
Falha no AddRecipe restaura slot, contador e currentRecipe. Sucesso preserva
currentRecipe de outro registrador. ACK aguarda seis builders de lookups;
retry de builder nao duplica a receita ja adicionada.

## Evidencia automatizada — limites claros

`tests/native_recipes/native_recipes_test.cpp` compila com BL_HAVE_QUICKJS=0 e
liga os **registradores reais de producao** e o **adapter IL2CPP de producao**,
sem QuickJS ou mock de RecipeLoader/ModRecipe. O host fornece metadata,
alocacoes e metodos Terraria simulados; portanto nao prova comportamento do
Terraria no celular. Executa reserva de IDs/chaves/duplicatas, tile ainda nao
instalado, estacoes locais e intermod, ingrediente carregado depois, receita
vanilla, commit/rollback, retry de lookups, isolamento, arrays errados,
alocacao recusada, pilha invalida, campo Single, offset fora do objeto,
assinatura ausente e limite de capacidade. Variantes usam processos separados
para exercer novamente a resolucao do contrato. ASan/UBSan no host aprovados;
alocacoes deliberadas do host nao modelam o GC do jogo.

`native_item_binding_test --recipes-only` liga NativeBasicItems de producao
sem QuickJS, aplica defaults reais do adapter a objetos do host e enfileira
receitas com ingredientes/estacoes ausentes. Nao e teste do boot MonoVM nem
registro de uma bancada colocavel. CI tambem mantem o teste JS legado e
verifica que nativeRecipes=true impede a passagem de receitas C# pelo JS.

## Android ainda obrigatorio

1. Abrir ExampleMod 0.1.36 e conferir recipes de ExampleItem/Pickaxe/Hamaxe.
2. Com ingredientes e bancada vanilla, verificar menu, Guia, material e craft.
3. Mod C# independente com AddTile("UID/EstacaoJS") e AddIngredient("UID/ItemJS"):
   verificar ausencia/presenca da dependencia, log individual e ordem inversa.
4. Save/Exit, reabrir e confirmar tipos/chaves e receitas sem duplicacao.
5. Preservar equipamentos, prefixos, asas e armaduras da infraestrutura anterior.
6. Registrar FPS, estabilidade e ciclo de vida. Nenhuma medicao Android nova feita.

A bancada C# propria ainda NAO foi portada; seu teste de colocacao/destruicao e
persistencia nao pode ser substituido por teste de registro do TileRegistry.
Proximo passo continua sendo a bancada completa e as receitas originais das
armaduras. Nao trocar por estacoes vanilla para aparentar conclusao.

## Entrega experimental autorizada

O workflow da branch permite uma prerelease privada identificada como
EXPERIMENTAL / Lote A incomplete. Antes de enviar, exige os oito workflows
independentes aprovados para o SHA exato, alem dos testes e build do proprio
job. Falha, cancelamento, SHA diferente ou suite ausente recusam a entrega.
A release oficial do lote completo continua pendente. O ExampleMod 0.1.36 atualiza documentacao/matriz e conserva
51 fontes: esta etapa modifica infraestrutura e documentacao, nao adiciona ports.

O teste adicional registry_regression_test executa os registradores reais com
oito threads disputando 192 chaves por categoria. Confere duplicatas, nomes
iguais em mods/categorias diferentes, lookup reverso e reserva dos 64 itens
nao carregados, que nao pode satisfazer dependencia de ingrediente. Nao e
teste de salvar/abrir um mundo no Terraria.
