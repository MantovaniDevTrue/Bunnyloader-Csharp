# Bunny Loader C# 0.1.10.15 — SourceBolt com callback Mono real

A versão 0.1.10.14 foi validada no Android 14 ARM64: 120 chamadas de Probe.AI(int tick, int state) retornaram callback-verified, callbacksExecuted=120, result=-3, sem erro.

## O que mudou

- O host agora possui um hook nativo de Terraria.Projectile.AI, instalado apenas sob solicitação explícita.
- O hook filtra todos os projéteis por tipo antes de entrar no Mono e executa exclusivamente para SourceCSharpMod / SourceBolt.
- O método do assembly BunnyManagedProbe.dll é localizado uma única vez e reutilizado; não há pesquisa por reflexão nem gravação no disco durante os callbacks de gameplay.
- O retorno do AI gerenciado produz uma leve alteração angular de velocidade, limitada a 120 chamadas por instância e no máximo 2400 chamadas para o experimento inteiro.
- O comportamento vanilla / JavaScript roda primeiro e todos os outros projéteis permanecem no caminho anterior.
- Em caso de falha de execução gerenciada, a experiência se desativa e mantém o comportamento anterior dos projéteis.
- O status contabiliza spawns, callbacks, alterações de velocidade, erros e o último slot/estado.
- Não há execução automática ao iniciar Terraria nem quando outros mods são carregados.

## Como testar

Instale a versão experimental junto com o Source C# Test 0.1.0.

No editor execute o teste isolado e o teste em processo, como na versão anterior, até obter verified / 42:

```js
JSON.stringify(bl.csharp.managedProbe(), null, 2)
JSON.stringify(bl.csharp.managedWorkerStatus(), null, 2)
JSON.stringify(bl.csharp.managedGameProbe("CONFIRM_IN_PROCESS"), null, 2)
```

Após confirmar o Mono no jogo e entrar no mundo:

```js
JSON.stringify(bl.csharp.managedProjectileEnable("CONFIRM_SOURCE_BOLT"), null, 2)
```

Feche o editor, use a arma Source C# Wand e dispare vários SourceBolt. Depois execute:

```js
JSON.stringify(bl.csharp.managedProjectileStatus(), null, 2)
```

Esperado: enabled=true, type=1136 ou outro tipo de projétil registrado, spawns maior que 0, callbacks maior que 0, velocityWrites maior que 0 e errors=0. O tipo é obtido dinamicamente, nunca depende de ID fixo.

## Limites

Este é um teste de integração com o Terraria: o callback gerenciado executado é o método de demonstração Probe.AI dentro do APK. O arquivo SourceBolt.cs do mod ainda não está sendo compilado dinamicamente nem define a lógica AI. O mod continua registrado pelo interpretador restrito SetDefaults-literals-v1.

Para suportar arquivos .cs arbitrários sem compilação externa, ainda é necessário integrar um compilador Roslyn ou outro pipeline C# dentro do aplicativo, gerar assemblies de cada mod e vincular os métodos ao host. Mods tModLoader PC não são compatíveis automaticamente com BunnyLoader.Source.

Não foi alterado o cálculo da direção de disparo do jogador: permanece com a mira antes do disparo, por meio dos mecanismos existentes do Bunny. O callback apenas atua na velocidade de SourceBolt após a AI do jogo.
