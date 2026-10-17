# Teste Android de hooks universais: ExampleMod CSharp 0.1.44

Este e um pacote de diagnostico temporario para a APK Bunny Loader CSharp 0.1.10.16.90.29.83. Nao precisa instalar outra APK: basta atualizar o ExampleMod CSharp 0.1.42 para 0.1.44 com o mesmo ID.

Ao carregar, tenta registrar cinco hooks a partir do SDK CSharp puro. Todos preservam o metodo original, sem alterar retorno e sem executar efeitos extras. Cada hook se desliga automaticamente depois de duas chamadas bem-sucedidas. O antigo ExampleNativeAccess continua desativado. Use um mundo descartavel para o teste inicial.

Ao entrar no mundo, o ExampleWelcomePlayer escreve uma linha:

`C# Hook QA 0.1.44: H1 ARMADO | H2 E-7 | H3 ARMADO | H4 ARMADO | H5 E-7`

Legendas:
- `OK1` ou `OK2`: pelo menos uma chamada real interceptada com `original.Invoke()` e registrada.
- `ARMADO`: assinatura aceita, aguardando o metodo ser executado na thread do jogo.
- `E-5`: classe nao encontrada; `E-6`: metadados incompatíveis; `E-7`: metodo/ponte nao encontrado; `E-11`: timeout de instalacao; `E-12`: tentativa fora da janela de instalacao Mono. Esses codigos dependem da implementacao nativa e devem ser copiados no relatorio.
- `ERRO`: falha sem codigo reconhecido; consultar o log do jogo.
- `AGUARDANDO`: modulo ainda nao executado.

H1 = `HookInt32Zero`: `Terraria.Utilities.UnifiedRandom.Next()` (alvo da versao PC, pode nao estar declarado na classe IL2CPP do Android).
H2 = `HookVoidInt32`: `Terraria.WorldGen.QuickFindHome(int)`.
H3 = `HookVoidInt32Pair`: `Terraria.WorldGen.PlantCactus(int,int)`.
H4 = `HookInt32Triple`: `Terraria.WorldGen.countWires(int,int,int)`.
H5 = `HookVoidInt32Triple`: `Terraria.WorldGen.ConvertTile(int,int,int)`.

Nao trate ARMADO como callback validado. A lista de metodos deriva de referencias PC e a assinatura pode nao existir no Android. Se H2-H5 estiverem ARMADO e sem chamadas, nao fuerce acao de alterar tiles, explosoes ou geracao de mundo so para provocar o teste: envie o resultado de instalacao e ajustaremos os alvos a partir da evidencia. H1 pode ser acionado em jogo naturalmente; seu limite de duas chamadas protege o frame pacing.

Envie captura da linha H1-H5 e o relatorio Bunny Loader; confirme se a tela manteve 60 FPS. Apos os testes, volte a versao 0.1.42 para manter todos os hooks desligados.
