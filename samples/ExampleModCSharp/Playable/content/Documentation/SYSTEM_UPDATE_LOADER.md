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
| UpdateUI(nint gameTime) | Main.UpdateUIStates(GameTime) | Após o original, fora do menu |
| ModifyTimeRate(ref BunnyTimeRateValues rates) | Main.UpdateTimeRate e estágio ClockAdvance | Após defaults vanilla; preserva frações |

As **19 APIs de SystemUpdateHooks** têm implementação na fonte C#/C++.
Os sete estágios intermediários conservam as instruções ARM64 do Bunny
original, com checagem dos quatro opcodes por âncora. O método raiz
`DoUpdateInWorld` executa uma vez e os estágios internos são disparados
diretamente para o MonoVM, sem JS.

`UpdateUI` oferece `nint gameTime`, referência nativa transitória válida
somente enquanto o callback é executado. O mod não deve guardar esse ponteiro,
nem supor que ele seja uma instância gerenciada C#. É chamado somente após
`Main.UpdateUIStates` fora do menu do jogo.

`ModifyTimeRate` recebe `ref BunnyTimeRateValues` com `TimeRate`,
`TileUpdateRate` e `EventUpdateRate`, todos `double`. O host valida o
tipo real dos campos IL2CPP, rejeita números negativos/não finitos, preserva
as frações de tiles e eventos, e altera o relógio pela etapa nativa
`ClockAdvance`. A ponte usa sessão por atualização para impedir escrita
atrasada de um callback já expirado.

**Cobertura do código não equivale a validação no jogo.** Ainda faltam
testar os 19 pontos no Terraria Android 301720, os hooks concorrentes de
outros mods, JNI/MonoVM no aparelho, desempenho de 60 FPS e descarregamento
e reentrada em mundo. Uma assinatura IL2CPP que não corresponda à build é
rejeitada. Sem essas verificações, não há APK nem merge.

## Integração source-only do lote de 15

O contador `ExampleWorldUpdateCounter` está ativo no pacote e instala somente
seus dois hooks reais. Os exemplos que alteram taxas ou outros sistemas
permanecem opt-in. Mods sem itens também são suportados: o verificador Roslyn
produz um inventário `HOOK_MODULES`, inclusive para herança personalizada, e o
host usa esse inventário para iniciar o dispatcher MonoVM. Não é necessário
adicionar um item fictício nem mencionar `BunnyHookModule` em comentários para
ativar a detecção. A contagem de classes inclui os módulos verificados e
mantém separada a validação exata das identidades de conteúdo nativo.

O teste `VerifyStandaloneModule` compila um mod independente, descobre sua
subclasse herdada, registra pelo dispatcher normal, executa seu contador e
descarrega o hook. Os testes do registro de estágios usam um provider host
controlado; a execução das âncoras no Terraria continua pendente.

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
3. Verificar os 4 métodos universais, 7 âncoras internas, UI/tempo e o método raiz
   comparando a ABI no Terraria Android 301720; conferir registradores ARM64.
4. Verificar integração dos hooks com **dois mods ativos**, desligar um durante a
   sessão e confirmar o outro ainda ativo (sem duplo update do mundo).
5. Confirmar ordem dos 19 callbacks, quarentena de exceções, estabilidade em
   mundo/menu, ausência de erro de MonoVM, FPS e unload/reload.

**Estado:** 19/19 APIs no fonte; CI desktop não valida execução nativa Android.
Ainda não está autorizada a criação de APK. PR em rascunho.
