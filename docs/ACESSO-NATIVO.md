# Acesso ao Terraria pelo SDK C#: lote A

Núcleo experimental .70 que integra o acesso nativo C# da PR #43 com os hooks `int(int)` da .68 e a recuperação adaptativa validada na .69. Mantém o worker MonoVM, o orçamento e as correções de recuperação dos hooks. A lógica do mod permanece em C# e a ponte usa o IL2CPP diretamente, sem executar JavaScript. Os fontes do Bunny Loader e os acessos a jogador do ExampleModBunny são a referência.

## O que entra neste lote

| Área | Contrato |
| --- | --- |
| Tipos próprios | `BunnyVector2`, `BunnyRectangle`, `BunnyColor` e `BunnyValue` |
| Classes | `hooks.Native.Type("Terraria.Player")`; classes de referência não genéricas |
| Campos | Instância/estático, leitura e escrita explícita; valida tipo, tamanho, offset e readonly |
| Propriedades | Getter sem índice e setter de um argumento compatível; getter obrigatório |
| Métodos | Chamada por nome e tipos completos dos parâmetros; até quatro argumentos e retorno suportado |
| Sobrecargas | Assinatura exata, sem escolher a primeira por contagem |
| Leitura em lote | Um pedido para até 16 campos/propriedades previamente vinculados |
| Ciclo de vida | Escopo descartável do módulo e objetos válidos somente no callback atual |

Isso amplia **chamadas a métodos**. Para interceptação, o núcleo integrado suporta `void()` sem parâmetros e `int(int)`; o acesso tipado deste lote pode ser usado em ambos. Outras assinaturas de hook ainda são recusadas.

## Exemplo

```csharp
using BunnyLoader.Source;

public sealed class PlayerSnapshot : BunnyHookModule
{
    private readonly BunnyValue[] values = new BunnyValue[2];
    public static int Life;
    public static BunnyVector2 Position;

    public override void Load(BunnyHookScope hooks)
    {
        var access = hooks.Native;
        var player = access.Type("Terraria.Player");
        var life = player.Field("statLife", BunnyValueKind.Int32);
        var position = player.Field("position", BunnyValueKind.Vector2);
        var batch = access.Batch(life, position);

        hooks.HookVoid("Terraria.Player", "ResetEffects", (original, self) =>
        {
            original.Invoke();
            var target = BunnyNativeObject.FromHook(self);
            batch.Read(values, target);
            Life = values[0].AsInt32();
            Position = values[1].AsVector2();
        });
    }
}
```

O [exemplo compilável](../Split/Bunny-Loader/samples/ExampleModCSharp/Experimental/NativeAccess/ExampleNativeAccess.cs) também demonstra um campo estático e `FindBuffIndex(System.Int32)`. Fica fora de `Playable` para não acrescentar chamadas por frame ao mod padrão antes da medição no aparelho. Usar junto com outro hook C# no mesmo endereço continua sendo recusado pelo núcleo universal.

Para vincular uma propriedade ou um método:

```csharp
var center = player.Property("Center", BunnyValueKind.Vector2);
var findBuff = player.Method("FindBuffIndex", BunnyValueKind.Int32,
    false, "System.Int32");
```

Durante o callback:

```csharp
var target = BunnyNativeObject.FromHook(self);
BunnyVector2 centerValue = center.Read(target).AsVector2();
int slot = findBuff.Invoke(target, BunnyValue.FromInt32(24)).AsInt32();
```

Escrita deve ser pedida no vínculo, por exemplo `player.Field("statLife", BunnyValueKind.Int32, writable: true)`, e aplicada com `member.Write(BunnyValue.FromInt32(value), target)`. Ler um membro não concede permissão de escrita. `initonly` e constantes são recusados para escrita.

## Tipos e layouts

| Tipo IL2CPP | BunnyValueKind / conversão |
| --- | --- |
| Boolean | Boolean / FromBoolean, AsBoolean |
| SByte, Int16, Int32 | Int32 / FromInt32, AsInt32; os menores validam a faixa |
| Byte, UInt16, UInt32 | UInt32 / FromUInt32, AsUInt32; os menores validam a faixa |
| Int64, UInt64 | Int64 ou UInt64 / conversões de 64 bits |
| Single, Double | Single ou Double / conversões correspondentes |
| Enum com base inteira suportada | Kind da base real; assinatura do método usa o nome completo do enum |
| XNA Vector2; UnityEngine.Vector2 | Vector2 / FromVector2, AsVector2 |
| XNA Rectangle | Rectangle / FromRectangle, AsRectangle |
| XNA Color nos namespaces suportados | Color / FromColor, AsColor |

`BunnyValue` tem representação de 24 bytes; os structs C# não são tratados como classes nativas do Terraria. A ponte confere os campos reais e converte por componentes, incluindo Vector2 com offsets diferentes e Color em ABGR. `BunnyColor.PackedRgba` coloca R no byte menos significativo, seguido de G, B e A; a conversão para a ordem do jogo é responsabilidade da ponte.

Referências de objetos, strings, arrays, ponteiros, genéricos, argumentos `ref/out` e outros structs ainda não fazem parte deste contrato. O lote não disponibiliza `Main.tile`, `Main.LocalPlayer`, texturas, SpriteBatch, interfaces ou geração de mundo como objetos gerenciados. Esses são os próximos blocos de API.

## Threads, validade e custos

- Vínculos são preparados em `BunnyHookModule.Load`, no pump do jogo, e retornam IDs de metadados em um escopo. O mod reutiliza esses membros; não procura nomes de campos por quadro.
- Leituras estáticas individuais também podem ocorrer em `Load`. Acesso a instâncias, escritas, métodos e lotes exigem um callback universal ativo. Não há acesso livre a partir de outras threads, callbacks assíncronos antigos ou `SetDefaults`.
- `BunnyNativeObject.FromHook(self)` aceita somente o self atual. O nativo valida o ticket, a thread do worker e o self antes de enfileirar, e valida novamente na thread do jogo. A classe real precisa ser compatível com a classe vinculada. Guardar esse objeto para outra chamada é inválido.
- O worker libera o mutex Mono durante o pedido. A thread do jogo atende a operação enquanto aguarda o callback; chamadas ao jogo feitas por essa operação ignoram a reentrada de hooks universais para não esperar recursivamente pelo mesmo worker.
- Pedidos usam armazenamento fixo e cópias de valores. Nenhum ponteiro para um buffer gerenciado fica pendente no jogo. Metadados têm espera de até 2 segundos; pedidos durante callbacks usam até 5 ms, mantendo o orçamento externo de 3 ms do hook. Um pedido pendente vencido não é executado posteriormente.
- Prazos não interrompem um método IL2CPP que já começou. Getters/métodos podem alocar objetos encaixotados e executar código caro. Outros callbacks síncronos que dependam do worker mantêm as limitações de aninhamento do núcleo universal. Não usar operações longas ou que dependam dessa reentrada.
- Escritas e métodos têm efeitos no jogo: uma exceção posterior ou timeout do callback não desfaz efeitos já executados. Não é uma transação com rollback. O mod continua responsável pelos valores que escreve.
- Um lote publica os resultados no destino somente se todas as leituras terminarem com sucesso. A execução acontece numa sequência na thread do jogo; isso não é um snapshot atômico contra todas as outras threads da geração de mundo.
- Limites iniciais: 64 escopos ativos, 2048 vínculos ativos, 16 leituras por lote, quatro argumentos por método. `Dispose` e a saída do mundo invalidam vínculos; slots podem ser reutilizados com um novo token. Objetos nativos não são mantidos como referências persistentes entre frames.

`BunnyAccessException.Status` diferencia tipo incompatível, membro ausente, readonly, objeto vencido, indisponibilidade, timeout e exceção nativa. O status evita gravar dados interpretados pela largura errada. A compilação e os testes de host não comprovam o comportamento de cada assinatura do Terraria Android.

## Teste Android

O [Native Access Probe CSharp](../Split/Bunny-Loader/samples/NativeAccessProbe/README.md) combina o módulo canônico e um BunnyPlayer mínimo, sem alterar vida, posição ou buffs. O pipeline .69 gera um APK assinado V3 e esse pacote de fontes. Desative o antigo Universal Hooks Probe CSharp para evitar dois hooks no mesmo endereço.

No Editor C#, a linha `Acesso C#` mostra vínculos, leituras, escritas, métodos, lotes, falhas, esperas vencidas, último status e escopos ativos. `lotes` e `metodos` devem crescer com o teste; `escritas` permanece zero. A consulta manual é `Print(BunnyNativeScope.GetStatus());`. Conferir também os contadores de recuperação dos hooks e os FPS; sair e entrar no mundo exercita descarregamento.

## Verificação

```sh
python3 tests/run_native_access.py
mkdir -p build/native-access
g++ -std=c++20 -shared -fPIC -I app/src/main/cpp tests/native_access/sdk_mock.cpp -o build/native-access/libbunny.so
LD_LIBRARY_PATH="$PWD/build/native-access" dotnet run --project tests/native_access/NativeAccess.csproj -c Release
```

Executar em `Split/Bunny-Loader`. A suíte usa IL2CPP simulado para validar operações na thread correta, layouts, largura/faixa, propriedades, sobrecargas, exceções, batch e recusa de self vencido/forjado. A suíte C# compila o exemplo e verifica P/Invoke, tipos, descarregamento e 10 mil chamadas de batch+método sem nova alocação gerenciada observada no host. Não mede boxing IL2CPP, latência entre threads, FPS ou integração MonoVM/Android. O lote ainda requer build e ensaio controlado no Xiaomi.
