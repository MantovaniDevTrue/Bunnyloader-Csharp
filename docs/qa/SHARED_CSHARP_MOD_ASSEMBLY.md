# Bunny Loader C# — identidade compartilhada de assemblies

Base: Bunny Loader C# 1.0.21 QA, branch `integration/csharp-unified-20261024`.

## Sintoma reproduzido em Android (24/10/2026)

Com apenas `Five Loaders CSharp QA 0.1.1`, o MonoVM registrou
`QaUpdates=SystemUpdate C#: hooks=15 eventos=75405 falhas=0`, mas
`/lote` mostrou `mundo=False` e `updates=0/19`. O teste com os
dois mods havia mostrado resultados semelhantes. O carregamento do mod
isolado levou 765 ms com cache quente e sem recompilação.

## Origem identificada nos fontes

O carregamento independente por `Assembly.Load(byte[])`,
`Assembly.LoadFrom` e `AssemblyLoadContext.LoadFromStream`
criava identidades gerenciadas diferentes para a mesma DLL. Assim,
o estado `static` de `QaUpdates`, `QaWorld` e `QaBolt`
podia ser diferente em comandos, hooks, persistência e projéteis.

## Ajuste proposto para validação

`BunnyModAssemblyRegistry` fornece a mesma instância de `Assembly`
para todos os serviços do mesmo mod, com contexto coletável por
identidade de mod, verificação do conteúdo da DLL e leases com
contagem de referências. A troca de uma DLL enquanto outra rotina
ainda usa a versão anterior é recusada, evitando estados misturados.

Comandos, sistemas de mundo, hooks universais, entidades, Dust,
receitas, Angler, projéteis de armas, raridades, localização e hooks
de dano do jogador passam a usar o registro compartilhado. O SDK
permanece comum; não se adiciona uma ponte JavaScript.

Assemblies carregadas por stream podem ter `Assembly.Location`
vazio. Dust recebe a identificação do registro do próprio mod, sem
inventar caminho a partir do arquivo da DLL.

## Teste automatizado

O teste `tests/five_loaders/Probe/Program.cs` passa a compilar a
fonte C# original do pacote de QA e testar os caminhos reais de
`BunnyWorldMonoHost` e `BunnyCommandWorker`, além da identidade
compartilhada utilizada pelos hooks. Deve comprovar:

- Uma instância única de DLL e visibilidade dos contadores 1/19 e 19/19 no comando.
- Mundo ativo, contador de pontos e save/load observados pelo mesmo comando.
- Reset de contadores ao entrar em outro mundo.
- Isolamento entre dois mods e rejeição de DLL conflitante.
- Liberação das referências após descarregamento.
- Resolução do dono mesmo quando `Assembly.Location` é vazio.

O teste de host simula a alteração de contadores; ele **não**
prova que os 19 hooks nativos executam corretamente no aparelho.

## Critério para publicar uma QA posterior

Executar primeiro os testes de host e as regressões, sem publicar APK
antes deles. Em seguida, validar no Android o Five Loaders sozinho:
`/lote` dentro do mundo deve mostrar `mundo=True` e
`updates=19/19`, sem perda de FPS relevante. Validar também Gore
visível, botões de toque, persistência, depois ExampleMod C#
isolado e finalmente ambos ligados.

Manter a 1.0.21 como baseline. A ExampleShotgun ainda possui alerta
independente de timeout no disparo; compartilhar assemblies não
constitui correção desse prazo de callback.
