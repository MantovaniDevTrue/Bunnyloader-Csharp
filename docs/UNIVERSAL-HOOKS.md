# Hooks C# por método: núcleo experimental void()

Esta etapa permite escolher uma classe `Terraria.*` e um método `void()` sem parâmetros. Não contém QuickJS nem código de tModLoader. A base é a .62 consolidada; a alternância QuickJS da PR #35 permanece separada. Não há APK novo nesta mudança.

```csharp
using BunnyLoader.Source;

public sealed class MyHooks : BunnyHookModule
{
    public static long Calls;

    public override void Load(BunnyHookScope hooks)
    {
        BunnyHooks.Hook(hooks, "Terraria.Player", "ResetEffects", (original, self) =>
        {
            original.Invoke();
            Calls++;
        });
    }
}
```

O arquivo deve ficar entre os fontes `content/**/*.cs` do mod C#. O autoloader detecta candidatos durante a leitura que já calcula o cache. O registro é permitido somente durante `BunnyHookModule.Load`; tentar instalar durante um callback é recusado. O worker descobre subclasses concretas de `BunnyHookModule` no assembly compilado. O exemplo compilável está em [ExampleUniversalHooks.cs](../Split/Bunny-Loader/samples/ExampleModCSharp/Experimental/UniversalHooks/ExampleUniversalHooks.cs), fora de `Playable`: não adiciona uma sincronização por frame ao mod padrão antes da medição Android.

`self` é uma identidade nativa opaca (`nint`), válida apenas durante a chamada. Não é um `Terraria.Player` gerenciado nem uma API de acesso a campos. Não deve ser armazenada ou dereferenciada. `isStatic: true` seleciona métodos estáticos; nesses callbacks `self` é zero.

## Execução e limites desta etapa

- A resolução e instalação ocorrem no pump da thread do jogo; o callback permanece no worker MonoVM fixo. Não se anexa a thread do jogo ao Mono.
- O callback pode executar código antes e depois de `original.Invoke()`, ou omitir a chamada para substituir o método. O original executa na thread que entrou no hook, com o `MethodInfo*` recebido e o próximo elo nativo lido atomicamente. Uma segunda chamada ao original é recusada.
- O encontro entre as threads usa um mailbox fixo, sem frame de pilha compartilhado ou alocação C++ por chamada. O SDK usa uma struct para o original. A entrada Mono de retorno `void` escreve o resultado em `out int`, evitando boxing desse resultado.
- O mutex do runtime Mono é liberado enquanto o worker aguarda instalação/original e retomado antes de continuar C#. Não fica preso enquanto o Terraria executa o original.
- O orçamento inicial é 3 ms de espera gerenciada, excluindo o tempo do original. Se o mailbox estiver ocupado, o jogo segue pelo original. A partir da `.90.29.65`, um timeout isolado aciona um segundo de resfriamento: o host executa o original durante a pausa e pode retomar o hook depois. Três timeouts dentro de 30 segundos desativam esse hook; falhas não relacionadas a timeout continuam desativando imediatamente. O original só é usado como recuperação quando ainda não executou. O callback C# já iniciado não pode ser interrompido à força: seu resultado tardio é descartado e seu acesso ao original expira.
- A fila é de uma chamada síncrona por vez. Calls vindas de outra thread e hooks universais aninhados durante o original seguem pelo original. Outros callbacks síncronos especializados que dependam do mesmo worker podem atingir seus próprios timeouts durante esse original. Métodos que exigem esse aninhamento ainda não são suportados para uso real.
- `Dispose()` remove o delegate e desativa o despacho nativo. O trampolim permanece como passagem ao original até o processo encerrar, evitando liberar código enquanto alguém ainda o executa. Saída do mundo cancela o mailbox, desativa os hooks e agenda `UnloadAll` no worker; entrada em outro mundo registra novamente. Não implementa hot-reload de assemblies nem o hot-unload geral do loader.
- Até 64 alvos nativos distintos por processo e 64 callbacks ativos. Alvos desativados podem ser vinculados novamente. Um segundo hook C# ativo no mesmo endereço é recusado nesta etapa; os elos existentes do HookManager (inclusive JS) são preservados.
- Genéricos, instâncias de structs, construtores, métodos com parâmetros, `ref/out`, retornos e ambiguidades são recusados antes da instalação. ARM64 é a arquitetura inicial. A forma `Player.Update(...)` do rascunho inicial ainda não é suportada.

O Editor C# pode consultar `Print(BunnyHooks.GetStatus());`: chamadas, concluídas, mailbox ocupado, timeouts, falhas terminais e timeouts recuperados são contados separadamente. A consulta formata uma string somente quando solicitada.

## Validação

Os testes de host verificam o mailbox e a integração nativa com metadados IL2CPP simulados: assinatura, instância/estático, ordem, afinidade de thread do original, recusa de duplicatas, timeout, cancelamento, ticket vencido, unhook e novo vínculo. Os testes C# usam P/Invoke com biblioteca simulada e verificam delegates, exceções, vida útil do original, descarregamento do módulo e reutilização dos slots. O exemplo é compilado na mesma suíte. Dez mil chamadas do dispatcher C# não produziram novas alocações gerenciadas observadas no host; essa medição não cobre MonoVM/Android nem a latência entre threads.

```sh
python3 Split/Bunny-Loader/tests/run_universal_hooks.py
cd Split/Bunny-Loader
mkdir -p build/universal-hooks
g++ -std=c++20 -shared -fPIC tests/universal_hooks/sdk_native_mock.cpp -o build/universal-hooks/libbunny.so
LD_LIBRARY_PATH="$PWD/build/universal-hooks" dotnet run --project tests/universal_hooks/UniversalHooks.csproj -c Release
```

Essas verificações não substituem uma build Android, integração com o MonoVM real, coleta de GC e medição de FPS/latência no Xiaomi. O exemplo `ResetEffects` serve para ensaio controlado de frequência alta; ainda não deve ser tratado como aprovado para produção. A próxima etapa é essa integração e medição, antes de ampliar para parâmetros, `ref/out`, retornos, structs, sobrecargas e encadeamento de vários mods no mesmo alvo.
