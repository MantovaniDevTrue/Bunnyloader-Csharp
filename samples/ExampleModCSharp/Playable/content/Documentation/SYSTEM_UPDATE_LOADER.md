# SystemUpdateLoader C# — contrato do lote

Este loader é **C# puro**, usa `BunnyHookModule` / `BunnyHookScope` do BunnySourceSdk
e os hooks IL2CPP já existentes no Bunny Loader. Não registra callbacks via JavaScript.
Os nomes dos métodos Terraria foram conferidos no `SystemUpdateHooks.js` da própria
árvore `v1.0`; eles ainda exigem validação funcional em Terraria Android 1.4.5.8.6.

## O que existe

| Método C# (`protected override`) | Hook Terraria | Ordem |
| --- | --- | --- |
| PreUpdateEntities | estágio nativo EntitiesBegin | Antes de Main.DoUpdateInWorld |
| PostUpdateEverything | estágio nativo EntitiesEnd | Depois de Main.DoUpdateInWorld |
| PreUpdateProjectiles | Main.PreUpdateAllProjectiles | Antes do original |
| PreUpdateDusts | Dust.UpdateDust | Antes do original |
| PostUpdateDusts | Dust.UpdateDust | Depois do original |
| PreUpdateInvasions | Main.UpdateInvasion | Antes do original |
| PostUpdateInvasions | Main.UpdateInvasion | Depois do original |
| PostUpdateInput | Main.DoUpdate_HandleInput | Depois do original |
| PreUpdatePlayers | estágio nativo PlayersBegin | Antes da atualização dos jogadores |
| PostUpdatePlayers | estágio nativo PlayersEnd | Depois dos jogadores |
| PreUpdateNPCs | estágio nativo NPCsBegin | Antes dos NPCs |
| PostUpdateNPCs | estágio nativo NPCsEnd | Antes dos gores |
| PreUpdateGores | estágio nativo NPCsEnd | Após PostUpdateNPCs, antes dos gores |
| PostUpdateGores | estágio nativo GoresEnd | Depois dos gores |
| PostUpdateProjectiles | estágio nativo ItemsBegin | Após projéteis, antes dos itens |
| PreUpdateItems | estágio nativo ItemsBegin | Após PostUpdateProjectiles |
| PostUpdateItems | estágio nativo ItemsEnd | Depois dos itens |

Os **17 métodos acima** já possuem declaração C# e ponte nativa projetada.
O estágio C# usa as sete âncoras ARM64 da própria implementação do Bunny,
com validação exata dos quatro opcodes originais antes de instalar um hook.
Uma incompatibilidade na versão do Terraria aborta a instalação.

A ponte C++ mantém um método raiz `DoUpdateInWorld` e duas fases
(`EntitiesBegin` / `EntitiesEnd`) para evitar espera circular no MonoVM.
Os eventos intermediários correm antes de liberar o trampolim original.
A execução entra pelo dispatcher C# existente; o JavaScript não despacha
esses callbacks.

**Ainda não implementados (2 de 19):**

| Callback restante | Requisito |
| --- | --- |
| UpdateUI | Main.UpdateUIStates(GameTime), após o original e com filtro fora do menu |
| ModifyTimeRate | Main.UpdateTimeRate, taxa de mundo/tile/eventos e etapa ClockAdvance |

O UI exige assinatura `GameTime` que a ABI universal atual não oferece.
Alterar o tempo exige preservar frações e a taxa do relógio nativo;
nenhuma substituição aproximada por evento de frame será aceita.

**Validação até agora:** os testes no desktop não comprovam a ponte ARM64,
os registradores no aparelho, os pontos de interceptação na libil2cpp nem o
desempenho com mods concorrentes. APK e merge permanecem proibidos.

## Como escrever o mod

Crie uma classe pública derivada de `BunnyWorldUpdateLoader` em `content/`.
O dispatcher normal C# descobre essa classe automaticamente.

```csharp
using BunnyLoader.Source;

public sealed class MeuSistema : BunnyWorldUpdateLoader
{
    protected override void PreUpdateEntities()
    {
        // Antes do update de entidades do Terraria.
    }

    protected override void PostUpdateEverything()
    {
        // Depois do update vanilla; sem chamar Main.DoUpdateInWorld manualmente.
    }
}
```

Para testes, existem `ExampleWorldUpdateCounter.cs` e
`ExampleDustAndInvasionSystem.cs` no ExampleModCSharp.
**Os dois estão desligados por padrão** (`EnableUpdates => false`).
Troque para `true` apenas no pacote de QA, sem alterar a versão estável.

## Regras de segurança e desempenho

- Apenas métodos sobrescritos recebem hook; um mod sem override custa zero chamadas extras.
- Existe um hook físico por método, mesmo que dois mods C# usem o mesmo estágio.
- Cada callback tem isolamento de exceção, contadores e erro resumido em `DiagnosticStatus()`.
- Nos estágios nativos internos, os callbacks ocorrem antes do retorno ao
  trampolim ARM64; o método vanilla inteiro só é chamado uma vez.
- Nos hooks universais de método, o original é chamado uma vez, e os
  callbacks pós-original seguem protegidos por `finally`.
- O cadastro é realizado durante `Load`, não em cada frame; os listeners são arrays
  imutáveis trocados apenas quando um mod entra ou sai.
- O registry controla o dono **por estágio**, não pelo primeiro mod global.
- Ao sair um mod, todas as inscrições daquele BunnyHookScope são removidas
  antes do Dispose. O próximo mod assume só os estágios que de fato usa.
- Callbacks de escopos já desativados não são mais executados, mesmo caso um
  snapshot antigo ainda esteja em voo; a rotina OnUnloaded executa uma vez.
- Não varra itens, NPCs ou projéteis inteiros dentro destes callbacks.

## Critérios antes de qualquer APK

1. Compilar o `BunnySourceSdk` e os dois exemplos com o .NET 8, sem warnings ou erros.
2. Passar o projeto `tests/world_update_loader`, incluindo três sistemas
   simulados, ordem dos eventos, exceções, partilha de hooks e unload/rebind.
3. Verificar os 4 métodos universais, 7 âncoras internas e o método raiz
   comparando a ABI no Terraria Android 301720; conferir registradores ARM64.
4. Verificar integração dos hooks com **dois mods ativos**, desligar um durante a
   sessão e confirmar o outro ainda ativo (sem duplo update do mundo).
5. Confirmar ordem dos 17 callbacks, quarentena de exceções, estabilidade em
   mundo/menu, ausência de erro de MonoVM, FPS e unload/reload.

**Estado:** 17/19 APIs ligadas na fonte. `UpdateUI` e `ModifyTimeRate` ainda
pendentes. CI não substitui QA Android nem libera APK/merge.
