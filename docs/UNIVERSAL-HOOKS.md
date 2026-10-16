# Hooks C# por método: núcleo experimental void() e int(int)

Esta etapa permite escolher uma classe `Terraria.*` com método `void()` sem parâmetros ou, na .68 experimental, método `int(int)` com exatamente um parâmetro `System.Int32` e retorno `System.Int32`. Não contém QuickJS nem código de tModLoader. A linha Android `.90.29.70` integra acesso nativo tipado da PR #43 e retomada adaptativa da PR #44 e preserva a alternância QuickJS, o Editor C# independente e o sistema de recuperação de timeout. O teste de continuidade no dispositivo ainda é necessário.

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

## Assinatura experimental int(int) da .68

```csharp
using BunnyLoader.Source;

public sealed class IntHooks : BunnyHookModule
{
    public override void Load(BunnyHookScope hooks)
    {
        hooks.HookInt32("Terraria.ExampleType", "ExampleMethod",
            (original, self, value) =>
            {
                int vanilla = original.Invoke();
                return vanilla + 1;
            });
    }
}
```

Os nomes `ExampleType` e `ExampleMethod` são placeholders: é obrigatório escolher um método existente com assinatura exata `System.Int32 Method(System.Int32)`. O carregador rejeita os demais alvos em vez de forçar uma chamada com ABI incompatível. `original.Invoke()` chama a implementação vanilla na thread do jogo e devolve o `int` original. O callback pode retornar outro `int`; se não chamar o original, substitui totalmente o resultado. Parâmetro de entrada é somente leitura nesta etapa. Um erro ou timeout faz o jogo usar o resultado vanilla quando necessário, mantendo o circuito de recuperação da .67.

## Execução e limites desta etapa

- A resolução e instalação ocorrem no pump da thread do jogo; o callback permanece no worker MonoVM fixo. Não se anexa a thread do jogo ao Mono.
- O callback pode executar código antes e depois de `original.Invoke()`, ou omitir a chamada para substituir o método. O original executa na thread que entrou no hook, com o `MethodInfo*` recebido e o próximo elo nativo lido atomicamente. Uma segunda chamada ao original é recusada.
- O encontro entre as threads usa um mailbox fixo, sem frame de pilha compartilhado ou alocação C++ por chamada. O SDK usa uma struct para o original. A entrada Mono de retorno `void` escreve o resultado em `out int`, evitando boxing desse resultado.
- O mutex do runtime Mono é liberado enquanto o worker aguarda instalação/original e retomado antes de continuar C#. Não fica preso enquanto o Terraria executa o original.
- O orçamento inicial é 3 ms de espera gerenciada, excluindo o tempo do original. Se o mailbox estiver ocupado, o jogo segue pelo original. A partir da `.90.29.65`, um timeout isolado aciona um segundo de resfriamento: o host executa o original durante a pausa e pode retomar o hook depois. Três timeouts dentro de 30 segundos suspendem o hook por 30 segundos, com fallback vanilla durante a pausa. Ao final da suspensão, o hook tenta novamente e a janela de timeouts é reiniciada. Novas sequências de timeouts voltam a suspender o hook; falhas não relacionadas a timeout continuam desativando imediatamente. O original só é usado como recuperação quando ainda não executou. O callback C# já iniciado não pode ser interrompido à força: seu resultado tardio é descartado e seu acesso ao original expira. Na `.90.29.66`, uma conclusão tardia, mesmo que gere uma exceção ao tentar chamar o original vencido, não remove o registro C#. Apenas uma falha confirmada enquanto o ticket está válido desativa o hook.
- A fila é de uma chamada síncrona por vez. Calls vindas de outra thread e hooks universais aninhados durante o original seguem pelo original. Outros callbacks síncronos especializados que dependam do mesmo worker podem atingir seus próprios timeouts durante esse original. Métodos que exigem esse aninhamento ainda não são suportados para uso real.
- `Dispose()` remove o delegate e desativa o despacho nativo. O trampolim permanece como passagem ao original até o processo encerrar, evitando liberar código enquanto alguém ainda o executa. Saída do mundo cancela o mailbox, desativa os hooks e agenda `UnloadAll` no worker; entrada em outro mundo registra novamente. Não implementa hot-reload de assemblies nem o hot-unload geral do loader.
- Até 64 alvos nativos distintos por processo e 64 callbacks ativos. Alvos desativados podem ser vinculados novamente. Um segundo hook C# ativo no mesmo endereço é recusado nesta etapa; os elos existentes do HookManager (inclusive JS) são preservados.
- Genéricos, instâncias de structs, construtores, `ref/out`, `float`, referências, múltiplos parâmetros e assinaturas de retorno diferentes de `void()` ou `int(int)` são recusados antes da instalação. O instalador usa os metadados IL2CPP para distinguir as assinaturas permitidas. ARM64 é a arquitetura inicial. A forma `Player.Update(...)` do rascunho inicial ainda não é suportada.

O Editor C# pode consultar `Print(BunnyHooks.GetStatus());`: chamadas, concluídas, mailbox ocupado, timeouts, falhas terminais, timeouts recuperados e `atrasadosDescartados` são contados separadamente. O painel também mostra `atrasados`, `suspensoes` e `retomadas`, indicando conclusões vencidas descartadas sem desmontar o hook e o progresso da recuperação automática. A consulta formata uma string somente quando solicitada.

## Validação

Os testes de host verificam o mailbox, a recuperação de conclusões tardias e a integração nativa com metadados IL2CPP simulados: assinatura, instância/estático, ordem, afinidade de thread do original, recusa de duplicatas, suspensão por três timeouts, reativação automática, cancelamento, ticket vencido, unhook e novo vínculo. Os testes C# usam P/Invoke com biblioteca simulada e verificam delegates, exceções, vida útil do original, descarregamento do módulo e reutilização dos slots. O exemplo é compilado na mesma suíte. Dez mil chamadas do dispatcher C# não produziram novas alocações gerenciadas observadas no host; essa medição não cobre MonoVM/Android nem a latência entre threads.

```sh
python3 Split/Bunny-Loader/tests/run_universal_hooks.py
cd Split/Bunny-Loader
mkdir -p build/universal-hooks
g++ -std=c++20 -shared -fPIC tests/universal_hooks/sdk_native_mock.cpp -o build/universal-hooks/libbunny.so
LD_LIBRARY_PATH="$PWD/build/universal-hooks" dotnet run --project tests/universal_hooks/UniversalHooks.csproj -c Release
```

Essas verificações não substituem uma build Android, integração com o MonoVM real, coleta de GC e medição de FPS/latência no Xiaomi. O exemplo `ResetEffects` já foi testado no Xiaomi com a .67: chamadas crescentes durante sessão prolongada e recuperações sem falhas terminais. A ABI `int(int)` da .68 e o acesso tipado da .69 ainda requerem verificação no Terraria Android antes de produção; a seguir estão `ref/out`, outros tipos, múltiplos parâmetros e encadeamento de mods no mesmo alvo.

## Acesso tipado durante callbacks

O [lote de acesso nativo](ACESSO-NATIVO.md) permite transformar o `self` opaco em `BunnyNativeObject.FromHook(self)` e consultar membros previamente vinculados por `hooks.Native`. O objeto é limitado à chamada atual; não autoriza dereferenciar o ponteiro nem reter um objeto do Terraria entre callbacks. Esse acesso é válido nos callbacks `void()` e `int(int)`.
