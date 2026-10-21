# SystemUpdateLoader C# — contrato do lote

Este loader é **C# puro**, usa `BunnyHookModule` / `BunnyHookScope` do BunnySourceSdk
e os hooks IL2CPP já existentes no Bunny Loader. Não registra callbacks via JavaScript.
Os nomes dos métodos Terraria foram conferidos no `SystemUpdateHooks.js` da própria
árvore `v1.0`; eles ainda exigem validação funcional em Terraria Android 1.4.5.8.6.

## O que existe

| Método C# (`protected override`) | Hook Terraria | Ordem |
| --- | --- | --- |
| PreUpdateEntities | Main.DoUpdateInWorld | Antes do original |
| PostUpdateEverything | Main.DoUpdateInWorld | Depois do original |
| PreUpdateProjectiles | Main.PreUpdateAllProjectiles | Antes do original |
| PreUpdateDusts | Dust.UpdateDust | Antes do original |
| PostUpdateDusts | Dust.UpdateDust | Depois do original |
| PreUpdateInvasions | Main.UpdateInvasion | Antes do original |
| PostUpdateInvasions | Main.UpdateInvasion | Depois do original |
| PostUpdateInput | Main.DoUpdate_HandleInput | Depois do original |

Os oito métodos formam o escopo **integral deste lote de atualização de mundo**.
Eles não pretendem substituir todos os métodos de `ModSystem` do tModLoader.
Outros loaders (`SystemWorldHooks`, `SystemDrawHooks`, etc.) não são declarados
implementados por esta PR.

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
- O original do Terraria é solicitado uma só vez por estágio e os eventos depois dele
  são executados no `finally`, inclusive se o original lançar exceção.
- O cadastro é realizado durante `Load`, não em cada frame; os listeners são arrays
  imutáveis trocados apenas quando um mod entra ou sai.
- Ao descarregar o dono do hook, o registro migra para o próximo mod ativo.
- Não varra itens, NPCs ou projéteis inteiros dentro destes callbacks.

## Critérios antes de qualquer APK

1. Compilar o `BunnySourceSdk` e os dois exemplos com o .NET 8, sem warnings ou erros.
2. Passar o projeto `tests/world_update_loader`, sem carregar biblioteca Android no desktop.
3. Verificar em revisão os nomes e assinaturas dos cinco alvos nativos (os nomes
   vêm de `SystemUpdateHooks.js`, mas o ABI real depende do Terraria Mobile).
4. Verificar integração dos hooks com **dois mods ativos**, desligar um durante a
   sessão e confirmar o outro ainda ativo (sem duplo update do mundo).
5. Confirmar ordem dos oito callbacks, quarentena de exceções, estabilidade em
   mundo/menu, ausência de erro de MonoVM, FPS e unload/reload.

**Estado:** implementação do SDK feita; validação CI e QA Android pendentes.
Não gerar APK, não declarar o loader validado no jogo e não mesclar antes de concluir
os itens de verificação relevantes. CI desta PR só compila/executa testes desktop.
