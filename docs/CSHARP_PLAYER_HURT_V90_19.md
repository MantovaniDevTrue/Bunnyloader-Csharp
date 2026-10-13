# Bunny Loader C# — Player Hurt API

## Objetivo

Expor callbacks C# escritos pelo autor do mod, em vez de limitar o comportamento a constantes pré-calculadas. O formato segue a ideia de eventos de combate do ExampleModBunny e do código JavaScript do Bunny Loader, sem depender da API do tModLoader.

## Contrato C# preparado

`BunnyPlayer.ModifyHurt(bool accessoryEquipped, BunnyHurtModifiers modifiers)`

Chamado antes de o dano entrar no `Player.Hurt`. O objeto será mutável e comportará dano recebido, direção, PvP, crítico, silêncio, possibilidade de esquiva, vida atual, vida máxima e cancelamento. O código do mod decidirá as transformações de forma dinâmica.

`BunnyPlayer.OnHurt(bool accessoryEquipped, BunnyHurtInfo info)`

Chamado apenas depois de um dano positivo efetivamente aplicado. Disponibilizará dano final, dano de origem, direção, vida restante, PvP e crítico, preservando o estado da instância C# entre eventos.

`ExampleVitalPlayer` contém um exemplo que reduz em 25% o dano não-PvP enquanto o acessório está equipado e contabiliza golpes recebidos.

## Estado de integração

- Contratos C# e verificação das assinaturas: implementados na branch experimental.
- Teste de código C# com estado mutável e regras de equipamento/PvP: incluído no Smoke Test.
- Conversão dos argumentos reais de `Player.Hurt` para os objetos C#: **pendente**.
- Despacho por golpe na MonoVM: **pendente**.
- Alteração de dano no jogo Android e notificação após o golpe: **não verificados**.
- Compatibilidade com hooks de `Player.Hurt` já usados por mods JavaScript: **precisa ser preservada**.

Não publicar como recurso funcional para mods até o fluxo completo passar em teste Android.

## Integração desejada

1. Adotar a assinatura `Player.Hurt` presente no Bunny Loader JS como origem verificada do evento, preservando os parâmetros e a ordenação com outros mods.
2. Garantir que `ModifyHurt` termine antes de chamar o método original e que `OnHurt` receba o dano realmente aplicado depois da chamada.
3. Fazer despachos gerenciados em uma única thread MonoVM controlada, evitando chamadas simultâneas e travamentos.
4. Nunca bloquear a thread principal indefinidamente. Se a execução C# estiver indisponível ou ultrapassar um limite temporal seguro, manter o comportamento original do Terraria e registrar a falha.
5. Testar, com ExampleMod C#, golpes em inimigos comuns, lava, queda, crítico, PvP, equipamentos trocados, morte e renascimento, compatibilidade JS e 60 FPS.

Este checkpoint é somente a base da API, não um APK que já executa esses callbacks na jogatina.
