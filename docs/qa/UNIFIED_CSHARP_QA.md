# Bunny Loader C# Unificado — QA Mobile 301720

Aplicativo único: `com.bunnyloader.csharptest`, versão `1.0.24-csharp-unified-qa`. Este APK reúne a base Dust/Invasion 1.0.11 e os cinco loaders novos, com uma só MonoVM e código-fonte C# editável.

Este é um **APK de QA**, não uma versão estável. O `SIGABRT` ao entrar no mundo observado na antiga Five QA.3 ainda precisa de validação/correção real. A compilação no GitHub não demonstra que funciona no aparelho.

1. Substitua o Bunny C# Test anterior pelo APK unificado e importe `examplemodcsharp-0.1.65.bl` e `fiveloaderscsharp-0.1.1.bl`. Não use APKs Five QA paralelos. Faça backup dos mundos.
2. Comece com o ExampleMod C# sozinho. Abra um mundo de QA. Teste cajado/minion, arma/tiro na mira, Dust Charm de dia e Beacon de invasão na superfície durante o dia. Verifique visualmente projéteis, efeitos, NPCs e FPS.
3. Volte ao menu, desative ExampleMod e teste o mod Five Loaders sozinho. Crie um mundo de QA, execute `/lote`, teste a varinha de Gore e os botões de toque no editor. O resultado esperado inclui dano 40 e 19/19 callbacks.
4. Execute `/lote add` três vezes, salve e saia, reabra o mesmo mundo e verifique os pontos. Faça outra tentativa com os dois mods habilitados somente após os testes individuais.
5. Caso ocorra SIGABRT, exporte o relatório completo de diagnóstico, inclusive a seção de abort nativo. Não insista em mundos de progresso real.

Confirme: entrada no mundo, chat `/lote`, contadores de toque e updates, pontos salvos, fragmentos Gore, Dust, invasão e FPS. O port C# não usa JavaScript como motor dos loaders portados.

## Diagnóstico do toque C# (QA5)

Com FiveLoadersCSharp 0.1.1 habilitado, atribua **QA dos cinco loaders / TestarLote** a um botão do editor nativo e salve o layout. Entre no mundo, feche o chat, pressione o botão e execute `/lote`. A linha `TouchHost` traz:

- `registro=ok`: a entrada gerenciada do botão foi criada;
- `host=Ready`: hooks IL2CPP e host de entrada estão ativos;
- `bound=True`: o controle está associado ao perfil ativo;
- `triggerFrames>0`: a MonoVM executou `ProcessTriggers`;
- `press>0`, `hold>0`, `duplo>0`: o botão respondeu aos gestos.

Se `press=0`, envie a linha `TouchHost` completa com o relatório do aplicativo. Teste separadamente pressionar, segurar, tocar duas vezes e atribuir a ação dentro de um grupo. O diagnóstico não prova por si só que o toque funcionou; a confirmação exige execução no Android.

## Leitura do TouchLayout (QA 1.0.24)

Depois de atribuir **QA dos cinco loaders / TestarLote** a um botão físico do perfil ativo, saia do editor, pressione o botão e execute `/lote`. A linha `TouchLayout` exibe `slots`, `parsed`, `direct` e `groups` da varredura nativa do perfil.

- `slots=0`: nenhum perfil/controle legível foi encontrado;
- `slots>0 parsed=0`: a estrutura de ações não foi lida corretamente;
- `parsed>0 direct=0 groups=0`: o perfil está legível, mas nenhuma ação C# foi encontrada (rever atribuição/perfil);
- `direct>0` ou `groups>0` com `bound=False`: há divergência entre o snapshot do perfil e o estado observado do botão;
- `bound=True` com `press=0`: investigar o hook de entrada nativa do botão, sem presumir falha da MonoVM.

O parser nativo não deve descartar uma ação direta válida quando o estilo visual do slot não for legível. A correção não atribui botões automaticamente e não altera os controles vanilla. O funcionamento real exige validação no Android.
