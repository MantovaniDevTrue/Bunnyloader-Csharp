# Callbacks C# com contexto e pedidos de efeito tipados

Este é um **arquivo de callback para substituir o equivalente na ExampleModCSharp**, não um `.bmod` completo. Ele precisa de uma build do loader que inclua `BunnyLiveApi.cs` no `BunnySourceSdk.dll`; a APK .85/.85.1 original não possui esses tipos novos. O despacho continua usando o protocolo de efeitos da .85. A validação Android de efeitos da .85.1 permanece necessária.

Copie `content/Common/Live/ExampleWaveBoltLiveEvents.cs` sobre o arquivo de mesmo caminho do seu exemplo. Preserve a arma, o projétil, a curva, os sprites e o manifest. Empacote o mod normalmente. O nome da classe e do arquivo continua vinculado a `ExampleWaveBolt`; para outro projétil, use `NomeDoProjetilLiveEvents`.

A lógica do exemplo é:

```csharp
public static BunnyNpcBuffRequest ChooseDebuff(BunnyNpcHitContext hit)
{
    var debuff = hit.Player.HealthFraction < 0.35f
        ? BunnyNpcDebuff.CursedInferno : BunnyNpcDebuff.Frostburn;
    return BunnyNpcBuffRequest.Create(debuff, hit.Npc.Life > 100 ? 240 : 120);
}
```

O adaptador `OnHitNPCV4` recebe os 12 valores existentes, cria o contexto e retorna `ChooseDebuff(hit).ToCallbackResult()`. `ChooseDebuff` é lógica do exemplo, não um novo callback que o loader descubra automaticamente. Os métodos legados `OnHitNPC` e `OnKill` permanecem para descoberta e fallback, retornando zero sem efeito.

## Contrato experimental

- `Npc` e `Player` são structs somente de leitura, copiadas do momento do hit. `Player` é o dono do projétil; não necessariamente o jogador local. Não há objetos, ponteiros nem consultas ao Terraria nesses tipos.
- `Call` é a sequência compartilhada da sessão; não é ID de entidade ou tick do mundo. `Damage` é o valor fornecido pelo callback nativo.
- `HealthFraction` fica entre 0 e 1; `MaxLife <= 0` produz zero. `FromV4` copia os valores e não certifica que um alvo continua vivo: essa validação pertence ao consumidor nativo.
- `BunnyNpcDebuff` inclui somente OnFire, CursedInferno, Frostburn e Ichor. Duração: 30–600 ticks. A unidade é tick do jogo, não milissegundo.
- `Create` lança `ArgumentOutOfRangeException` para valores inválidos. Use `TryCreate` para configuração externa; em falha, o resultado é `None`.
- `None`/`default` retorna zero. O pedido só tem efeito quando seu código é retornado de **OnHitNPCV4**. Retorná-lo de `OnKill` ou guardar a struct não agenda nada.
- É um pedido assíncrono, não uma confirmação de aplicação. Mantêm-se a FIFO64, a validade de 1,5 s, as verificações nativas do alvo/mundo e os descartes por saturação. Não há suporte a vários pedidos no mesmo retorno.
- O helper não remove o teto experimental de 25.000 callbacks e não adiciona multiplayer, acesso direto a entidades ou garantia de aplicação de buff.

## Testes

```sh
dotnet run --project Split/Bunny-Loader/experiments/csharp/BunnyLiveApi.Tests -c Release -- Split/Bunny-Loader/samples/TypedLiveCallbacks /tmp/bunny-buff-vectors.txt
g++ -std=c++20 -O2 -Wall -Wextra -Werror -I Split/Bunny-Loader/app/src/main/cpp Split/Bunny-Loader/tests/typed_live_api_protocol.cpp -o /tmp/bunny-api-protocol
/tmp/bunny-api-protocol /tmp/bunny-buff-vectors.txt
```

O teste usa o compilador Roslyn real do Bunny para compilar este arquivo, executa as duas decisões do exemplo, verifica o fallback, os limites/valores inválidos e compara todos os 2.284 pedidos válidos gerados pelo SDK com o decoder C++ usado pelo jogo. CI host não comprova aplicação de efeitos no Android.
