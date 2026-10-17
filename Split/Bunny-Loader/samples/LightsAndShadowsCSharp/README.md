# Lights And Shadows C# — 0.4.2.1

Port do Lights And Shadows de yiyang233 / AWEoghAwo, com formulas e assets originais do PC como referencia.

## Divisao de responsabilidades

O `content/main.cs` e dono de onze parametros de visual e desempenho: forca solar, limites e suavidade das sombras durante dia/noite, intensidade e qualidade dos raios, extracao e composicao do bloom, e ganho do blur. Sao propriedades C# nomeadas `Uniform_*` atribuidas em `SetDefaults()`, compiladas com o mod e lidas apenas na etapa de registro.

O arquivo `content/Effects/Lights.screenfx` vincula `@param.Nome` aos shaders do mod. A GPU executa as formulas por pixel em arquivos GLSL `.frag`, incluindo mascara azul, raios 30/20 taps, iluminacao e bloom com quatro passes Kawase. As texturas `ColorTex.png` e `ColorTex2.png` sao as originais. O `ColorTex_Alt.png` original fica preservado para um proximo lote.

O Bunny Loader e o dono generico do registro de mod, criacao de render targets, compilacao GLSL, conexao de parametros `@param.*` e leitura de informacoes do Terraria. As variaveis herdadas `@sun`, `@strength`, `@clock`, `@dayblend` e `@enabled` ainda existem no host para compatibilidade com as builds validadas .88/.88.1; a atualizacao de cena gerenciada e opcional e usa o trabalhador MonoVM assíncrono com fallback nativo.

Nenhuma chamada por quadro ao MonoVM e introduzida: os parametros C# sao vinculados uma vez ao registrar o efeito. Mantem a correcao de flicker de quadros e as transicoes suaves da versao .88.1.

## Empacotamento

`python3 package_mod.py` gera o `.bmod` somente de arquivos fonte C#, GLSL e texturas originais. Requer Bunny Loader C# .88.3 ou mais recente. A .88.2 não reconhece a diretiva SCENE; mantenha a versão 0.4.1 com esse Loader.

`content/Common/Lighting/` contem LightingSettings, LightingFrame e LightingCalculator.Compute com formulas extraidas do port PC. O calculo managed recebe snapshots ao vivo pela API experimental .88.3. O grafo declara SCENE|LightsAndShadowsCSharp|LightingCalculator|ComputeSceneWire e a thread MonoVM recebe instantâneos de tempo, superfície, posição da câmera e fase da Lua, até 10 vezes por segundo. Os resultados alimentam os uniforms com compensação nativa para o movimento entre amostras. Resultados atrasados são descartados e a iluminação nativa continua como fallback. Os dados opcionais de céu, bioma e gravidade ainda não têm todos os leitores IL2CPP implementados, portanto o port não tem fidelidade integral ao PC.

A fidelidade integral do mod de PC (todos os efeitos, biomas e opcoes) ainda esta em desenvolvimento. Primeiro validar com outro mod C# ativo, dia/noite, troca de biomas, subida ao espaco e FPS antes de mesclar.

## Hotfix 0.4.2.1

O tokenizador antecipado da .88.3 nao reconhece literals C# com aspas simples, por isso `wire.Split('|')` foi alterado para `wire.Split("|".ToCharArray())`. A alteracao preserva a semantica, a matemática C#, os 9 passes e os 4 alvos; nao exige novo APK.
