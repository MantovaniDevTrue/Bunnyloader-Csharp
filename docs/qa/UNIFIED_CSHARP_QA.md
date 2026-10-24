# Bunny Loader C# Unificado — QA Mobile 301720

Aplicativo único: `com.bunnyloader.csharptest`, versão `1.0.21-csharp-unified-qa`. Este APK reúne a base Dust/Invasion 1.0.11 e os cinco loaders novos, com uma só MonoVM e código-fonte C# editável.

Este é um **APK de QA**, não uma versão estável. O `SIGABRT` ao entrar no mundo observado na antiga Five QA.3 ainda precisa de validação/correção real. A compilação no GitHub não demonstra que funciona no aparelho.

1. Substitua o Bunny C# Test anterior pelo APK unificado e importe `examplemodcsharp-0.1.65.bl` e `fiveloaderscsharp-0.1.1.bl`. Não use APKs Five QA paralelos. Faça backup dos mundos.
2. Comece com o ExampleMod C# sozinho. Abra um mundo de QA. Teste cajado/minion, arma/tiro na mira, Dust Charm de dia e Beacon de invasão na superfície durante o dia. Verifique visualmente projéteis, efeitos, NPCs e FPS.
3. Volte ao menu, desative ExampleMod e teste o mod Five Loaders sozinho. Crie um mundo de QA, execute `/lote`, teste a varinha de Gore e os botões de toque no editor. O resultado esperado inclui dano 40 e 19/19 callbacks.
4. Execute `/lote add` três vezes, salve e saia, reabra o mesmo mundo e verifique os pontos. Faça outra tentativa com os dois mods habilitados somente após os testes individuais.
5. Caso ocorra SIGABRT, exporte o relatório completo de diagnóstico, inclusive a seção de abort nativo. Não insista em mundos de progresso real.

Confirme: entrada no mundo, chat `/lote`, contadores de toque e updates, pontos salvos, fragmentos Gore, Dust, invasão e FPS. O port C# não usa JavaScript como motor dos loaders portados.
