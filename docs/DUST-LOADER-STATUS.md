# DustLoader C# — andamento e critérios de entrega

## Fontes verificadas

Base de integração: `v1.0` do Bunny Loader C#. Referência do motor: `JonataOliveiraa/Bunny-Loader`, `app/src/main/cpp/content/dusts/ModDusts.{h,cpp}`. O motor original tem registro nativo de tipos, atualização/coloração/desenho de Dust, texturas do mod e recuperação da pilha de 6000 slots. Ele também possui a API JavaScript, que não será usada no caminho C#.

## Implementado nesta branch

- `BunnyDust`, `BunnyDustState`, `BunnyDustHooks`, `BunnyDustDefinition` e `BunnyDustRegistry` em C# independente.
- Identidade por nome, binding por ID nativo, validação antecipada de texturas e tipo de atualização.
- Sobrescritas opcionais `OnSpawn`, `Update`, `MidUpdate`, `GetAlpha` e `PreDraw`; nenhuma chamada por quadro a métodos não sobrescritos.
- Proteção por `slot + type + generation`, restauração de estado em erro e bloqueio do callback com problema apenas naquela instância.
- `ResetWorld`, `Release` e `Unload` distintos.
- ABI `ManagedDustState` de 64 bytes conferida por C# e C++.
- Testes puros de .NET/C++, executados em CI sem tarefas Gradle ou geração de APK.

## Ainda obrigatório antes de declarar completo

1. Importar/conciliar o Dust nativo do Bunny original com o port C#, incluindo `content/dusts`, ativação em `menu/Cheats.cpp` e `content/common/ModContent.cpp`, sem copiar arquivos atuais do upstream sobre os locais modificados.
2. Incluir `BunnyDust` em `SourceParser.cpp`, `SourceLoader.cpp` e identidades de conteúdo. Reservar e registrar os tipos na thread correta antes da entrada no mundo. Nenhum tipo deve ser identificado como registrado apenas por existir no SDK.
3. Incluir o tipo na descoberta gerenciada, em `CompiledAssemblyProbe.cs`, compilação por Roslyn e na resposta de defaults; conferir nome, textura, `UpdateType` e máscara de callbacks.
4. Criar o canal MonoVM C++/C# para lote de estados (com geração e limite por quadro), invocar os callbacks C# sem usar JS e devolver atualizações com identidade e validade conferidas. Nenhum callback C# pode bloquear a thread do jogo esperando o worker.
5. Adaptar `DustCallbacks` do motor original para receber os resultados C# independentes, corrigir o ciclo de vida dos slots e restaurar sempre o estado ao retornar ao Terraria; preservar a recuperação de `freeDust` e o comportamento de mods JS.
6. Resolver texturas de Dust por mod, frames/atlas e cores, com rejeição de assets faltantes e fallback fiel aos nativos.
7. Criar um exemplo C# testável, usando assets do ExampleModBunny com licença e caminhos corretos, e validar spawn, atualização, alpha, desenho, morte e reaproveitamento de slots no Android.
8. Validar coexistência com múltiplos mods C# + JS, troca de mundo, reload, ausência de vazamento de slots, 60 FPS quando possível, diagnósticos de falha e todas as builds de fontes relevantes.

**Regra de release:** manter a PR em rascunho, não mesclar como funcional e **não gerar APK** enquanto qualquer critério acima estiver pendente. Os testes de host validam somente contratos gerenciados/ABI, não a integração in-game.
