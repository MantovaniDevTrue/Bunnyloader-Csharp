# Hooks CSharp .84 — diagnostico real Android

ExampleMod CSharp **0.1.45** para Bunny Loader CSharp **0.1.10.16.90.29.84**.

O BunnyPlayer.OnEnterWorld e avaliado na fase de preparacao e nao reflete estados vivos do MonoVM. Nesta versao a mensagem de entrada indica que os estados H1-H5 devem ser consultados na secao "hooks universais C# (registro real MonoVM)" do relatorio Bunny Loader.

O runtime salva `universal-hooks-status.txt` quando solicita registro (queued), apos executar dispatcher (registered), ao sair do mundo (paused) ou apos falha (failed). Mostra o PID, horario, quantidade de modulos e diagnostico de cada modulo. `ARMADO` confirma apenas instalacao do hook; `OK1/OK2` confirma callback real; `E-5/E-6/E-7` indica alvo/sinatura invalida; `AGUARDANDO` sugere modulo nao inicializado.

H1 = int(): Terraria.Utilities.UnifiedRandom.Next(). H2 = void(int): Terraria.WorldGen.QuickFindHome(int). H3 = void(int,int): Terraria.WorldGen.PlantCactus(int,int). H4 = int(int,int,int): Terraria.WorldGen.countWires(int,int,int). H5 = void(int,int,int): Terraria.WorldGen.ConvertTile(int,int,int). Todos sao candidatos de PC, nao validados em Android.

Use mundo descartavel. Instale APK .84 e ExampleMod .45, entre, aguarde alguns segundos, saia, gere o relatorio e envie o trecho "hooks universais C#", junto ao FPS. Cada callback preserva original e se desativa apos duas chamadas; ResetEffects continua desligado. Nao tente forcar efeitos de worldgen. Ao fim volte ExampleMod 0.1.42 para jogatina normal.
