# Inventário oficial ExampleMod tModLoader → Bunny C#

Referência fixada: `tModLoader/tModLoader`, branch `1.4.4`, commit `130d914e622c1db30007e647d311cb00e4f7a3d1`, árvore `69e20b7caeadfae05f92bd8befeb07f3c21cd0dc`. O objetivo é portar comportamentos para APIs próprias e bridges reais do Bunny no Terraria Android 1.4.5.8.6; não carregar nem referenciar as assemblies do tModLoader no runtime Android.

Foram contabilizados **1.210 arquivos do ExampleMod**, incluindo **561 fontes C#**: **449 ativos** e **112 em Old**. O próprio `ExampleMod.csproj` exclui Old de Compile/EmbeddedResource/None. Os legados continuam no inventário para que seus comportamentos e substitutos atuais sejam auditados, sem classes duplicadas dentro do pacote do Bunny. A dependência `ExampleLibraryMod` também está listada separadamente (11 arquivos).

[Inventário JSON](tmodloader-example-inventory.json) registra caminhos, SHA de cada blob, tamanhos, papel e candidatos C# do Bunny. Coincidência de nome não prova port: nenhuma linha possui port integral certificado (`verifiedPort=false`); o estado de auditoria e registrado individualmente. Os demais arquivos também constam do documento, para preservar assets e metadados. A matriz existente dos exemplos JavaScript do ExampleModBunny permanece independente; nenhum estado de conclusão foi alterado por gerar este arquivo.

Próxima etapa: ler os fontes de cada grupo e decompor propriedades, hooks, execução nativa e assets. Comparar primeiro materiais, itens e receitas com o Lote A atual, conferindo comportamentos existentes para não reimplementar sistemas estáveis. Para cada lacuna, ampliar a API pública reutilizável, integrar o runtime, portar o grupo completo e registrar testes de contrato, registro e comportamento. Usar compilação/CI e Android como evidências distintas. A base tModLoader 1.4.4 não é uma prova de disponibilidade de um campo, método ou ABI no Terraria Android 1.4.5.8.6.

Não foram criados ports novos neste checkpoint: a revisão .88.8 corrigiu build e contratos de leitura do Lights, com validação Android ainda pendente. Este inventário impede omissões na expansão posterior, mas não oferece uma porcentagem artificial de implementação.


## Primeiro grupo de infraestrutura: receitas .89

A linha Content/ExampleRecipes.cs foi auditada e detalhada no JSON. Estado: **Parcial**, com grupos nativos, referencias tipadas, Clone de receitas Bunny e remocao implementados. O [relatorio](csharp-native-recipe-groups-v89.md) e o guia dentro do bmod registram os testes e todas as lacunas. Os demais arquivos conservam seu estado de auditoria; nenhum arquivo foi removido e nenhum port foi certificado integralmente.

Validacao de continuidade em 18/10/2026: CI .89 aprovado e usuario confirmou funcionamento do fluxo de receitas entregue no Android com ExampleMod 0.1.48. A linha ExampleRecipes continua Parcial; nenhum outro arquivo foi promovido. Ver [evidencias e escopo](validation/csharp-v89-android-2026-10-18.md). A meta de primeira compilacao <=5 s e FPS nao foi validada por esse relato.
