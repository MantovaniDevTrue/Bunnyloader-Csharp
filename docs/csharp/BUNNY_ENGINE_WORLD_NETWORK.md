# Bunny Engine — multiplayer de sistemas C# (lote 3)

Referência original: `ExampleModBunny/ExampleMod/content/Common/Systems/DownedBossSystem.js`.

O ExampleModBunny usa `NetSend(writer)` com `writer.WriteFlags(DownedBossSystem.downedExampleBoss)` e `NetReceive(reader)` com `reader.ReadFlags()`. A implementação em C# usa o mesmo comportamento e uma API própria, sem dependência de JavaScript ou tModLoader.

## Exemplo

```csharp
public override void NetSend(BunnyNetWriter writer)
{
    writer.WriteFlags(DownedExampleBoss);
}

public override void NetReceive(BunnyNetReader reader)
{
    bool[] flags = reader.ReadFlags();
    if (flags.Length != 1)
        throw new FormatException("Unexpected boss flags");
    DownedExampleBoss = flags[0];
}
```

O autor pode usar `WriteFlags` (até 16 bools por chamada), `WriteByte`, `WriteInt32` e `WriteString`, com seus métodos de leitura correspondentes. O payload total é limitado a 96 bytes por sistema por revisão. A API recusa tamanhos incorretos, buffers truncados e pacotes com bytes não lidos.

## Fluxo de execução

1. O host executa `NetSend` nos sistemas C# participantes durante o tick gerenciado existente, cerca de uma vez a cada 30 frames.
2. Apenas quando o payload muda, sua revisão aumenta. O host publica um pacote `WS1`, isolado da rede de invasões `INV1`.
3. A camada de rede nativa usa as sessões e o mecanismo de nonce/join já implementado para as invasões. Mensagens aos clientes usam `BLWS2`. O host mantém estado e último ack por peer, entregando no máximo dois pacotes no total por ciclo do bombeamento nativo, compartilhado com as invasões.
4. O cliente verifica o nonce associado à sessão, identidade do mod, nome do sistema, formato, revisão crescente e host session estável. Apenas então chama `NetReceive` em C#.
5. O cliente **não** escreve estado autoritativo no arquivo local e não pode publicar estados no canal host.

## Limitações

- A camada de rede é específica para os sistemas de mundo; não constitui um `ModPacket` universal.
- A payload é pequena de propósito, por causa do transporte autenticado de mensagens do jogo. Redes grandes exigirão um protocolo separado com fragmentação e confirmação.
- A revisão monotônica é por sessão, não um identificador de objetos do Terraria.
- O estado sincronizado atualmente chega até um tick gerenciado depois do recebimento do pacote.
- O empacotamento C#, a compilação e os testes de protocolo podem passar sem validar comunicação real entre dois aparelhos. A validação Android multiplayer permanece obrigatória.
- MonoVM/Roslyn continuam sendo o runtime C#; não foi criada uma nova VM.

## Testes

```sh
c++ -std=c++20 -O2 -Wall -Wextra -Werror -Iapp/src/main/cpp experiments/csharp/BunnyWorldSystemsSmoke/WorldNetProtocolSmoke.cpp -o /tmp/bunny-world-net
/tmp/bunny-world-net
dotnet run --project experiments/csharp/BunnyWorldSystemsSmoke/BunnyWorldSystemsSmoke.csproj -c Release
```
