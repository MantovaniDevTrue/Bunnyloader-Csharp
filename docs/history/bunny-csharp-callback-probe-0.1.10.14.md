# Bunny Loader C# — validação experimental de callbacks (0.1.10.14)

A versão 0.1.10.13 foi validada em Android 14 ARM64: o Mono 8.0.23 inicializou no próprio processo do Terraria e executou o método gerenciado Probe.Run(19, 23), retornando 42. O teste isolado também passou.

Esta etapa valida a invocação repetida de um método C# chamado AI(int tick, int state), sem instalar hooks automáticos de IA e sem afetar os mods JavaScript ou NativeAOT.

## Implementação

- BunnyManagedProbe.dll inclui Probe.AI(int tick, int state), que atualiza um estado inteiro.
- ManagedRuntime.cpp reutiliza o domínio Mono previamente inicializado, localiza o método uma vez e executa até 120 chamadas por solicitação manual.
- O método retorna o novo estado; o host verifica cada resultado com uma previsão independente e registra callbacksExecuted.
- O comando não é chamado no loop de atualização do Terraria. Não representa ainda a IA de um projétil de mod.
- Erros ficam no status e checkpoint do Mono para diagnóstico; esse teste continua experimental.

## Como testar

No Bunny Loader 0.1.10.14 experimental, primeiro execute managedProbe() e confira managedWorkerStatus() com verified/result=42, depois execute managedGameProbe("CONFIRM_IN_PROCESS") no menu, como na versão anterior. Com essa etapa aprovada, execute:

```javascript
JSON.stringify(bl.csharp.managedCallbackProbe(120), null, 2)
```

Esperado: phase="callback-verified", callbacksExecuted=120, error="". O campo result mostra o estado final calculado por C#. Não disparar automaticamente no jogo antes do teste.

## Limitações e próximos passos

Não existe ainda compilação automática de arquivos .cs dos mods. O SourceCSharpMod atual usa o interpretador literal SetDefaults-literals-v1. Após este teste, o próximo desenvolvimento será conectar o dispatcher gerenciado a um hook real de projétil, com registro por tipo e cache dos métodos, e só depois integrar compilação .cs offline dentro do APK. Qualquer hook de uso de arma deverá sincronizar a direção do jogador com a mira antes do disparo.
