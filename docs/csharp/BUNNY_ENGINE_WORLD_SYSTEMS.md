# Bunny Engine — sistemas C# de mundo (lote 1)

Este lote introduz `BunnyWorldSystem` no SDK C# nativo do Bunny Loader, tomando como referência estrutural `ExampleModBunny/content/Common/Systems/DownedBossSystem.js`. Não utiliza APIs do tModLoader nem converte JavaScript para executar o sistema.

## Exemplo

```csharp
using BunnyLoader.Source;

namespace MeuMod;

public sealed class MeuSistema : BunnyWorldSystem
{
    public static bool BossDerrotado { get; private set; }

    public override void OnWorldEnter(in BunnyWorldContext world)
    {
        BossDerrotado = false;
    }

    public override void OnWorldTick(in BunnyWorldContext world)
    {
        if (!world.IsAuthoritative)
            return;
    }

    public override void OnWorldExit()
    {
        BossDerrotado = false;
    }
}
```

Não é necessário editar `main.cs` nem adicionar itens fictícios. A descoberta de classes agora conhece `BunnyWorldSystem` e atribui `SYSTEM:<nome>` como identidade exclusivamente C#, sem pedir um tipo nativo de item, NPC ou projétil. O exemplo completo está em `samples/ExampleModCSharp/Playable/content/Common/Systems/ExampleWorldProgressSystem.cs`.

## Contrato implementado

- `OnWorldEnter` ocorre na primeira atualização de uma sessão de mundo reconhecida pelo host.
- `OnWorldTick` recebe um snapshot imutável, não objetos IL2CPP. O agendador existente envia um lote de 30 quadros aproximadamente a cada 30 frames, não uma chamada por frame.
- `OnWorldExit` é emitido quando outra sessão substitui a anterior ou quando o despachante recebe `ResetWorld()`. A integração nativa de fechamento imediato do mundo ainda precisa de validação.
- O registro respeita o UID do mod, usa ordem estável de nomes de classe, suporta até 32 sistemas por mod e impede que exceções de um sistema interrompam os outros. O sistema que falhou não recebe novas atualizações naquela sessão.
- Modos cliente e servidor são informados por `NetMode`; decisões de autoridade devem usar `IsAuthoritative`.
- Uma sessão mais antiga não substitui o estado de uma sessão mais nova.
- O limite atual do despachante nativo permanece em 16 mods vinculados.

## Verificações automatizadas

```sh
c++ -std=c++20 -O2 -Wall -Wextra -Iapp/src/main/cpp app/src/main/cpp/experiments/csharp/SourceParser.cpp experiments/csharp/BunnyWorldSystemsSmoke/SourceParserSmoke.cpp -o /tmp/bunny-world-parser
/tmp/bunny-world-parser

dotnet run --project experiments/csharp/BunnyWorldSystemsSmoke/BunnyWorldSystemsSmoke.csproj -c Release
```

Os testes verificam descoberta sem conteúdo nativo, idempotência de registro, entrada, atualização, isolamento de exceções, nova sessão, descarte de sessão antiga e reset. O workflow `check-bunny-world-systems.yml` pode executá-los em CI.

## Limitações que não devem ser omitidas

Este é um primeiro componente do motor de mods, e não uma VM C# substituta. A execução continua baseada no MonoVM/Roslyn do projeto, com integração C++/IL2CPP. Estado de progressão ainda não é persistido em arquivos de mundo nem sincronizado automaticamente em multiplayer. Os métodos de mundo não oferecem acesso arbitrário à memória IL2CPP e não substituem a AI por tick de NPCs ou projéteis. A reversão completa de recursos no unload e a verificação real no Android estão pendentes. Nenhum APK deve ser apresentado como validado com base apenas nesses testes.
