# Declaracao de conteudo e heranca C#

Requer um loader compilado com as mudancas de `codex/csharp-sdk-runtime-foundation`.
O atributo BunnyContent nao existe na APK .90.29.31. Este exemplo ainda nao foi
validado no Android e nao altera a contagem de ports completos do ExampleModBunny.

`SharedMaterial<T>` e uma base generica abstrata: nao recebe ID no Terraria.
`DeclaredMaterial` e a unica classe concreta registrada. Seu atributo declara
categoria, nome e textura antes de iniciar o MonoVM; `SetDefaults` executa a
implementacao C# herdada, e `AddRecipes` produz uma receita nativa.

A textura vem do ExampleItem do ExampleModBunny do repositorio. Uma terra
produz um material. O parser nativo e a verificacao Roslyn exercitam estes mesmos
fontes. Compare com o StarterMod se nao precisar de heranca.

Cada classe concreta precisa declarar seu atributo diretamente e manter seu
proprio override de `SetDefaults` (ou `SetStaticDefaults` para tiles).
Alterar o nome/textura declarados dentro do metodo gera erro de identidade.
Este exemplo cobre BunnyItem; defaults de armas/projeteis e outras categorias
ainda seguem as restricoes de literais do registro antecipado.

No Android: testar registro de um unico item, crafting, tooltip, save/reentrada,
ausencia de item extra para a base abstrata e convivencia com outro mod C#.
