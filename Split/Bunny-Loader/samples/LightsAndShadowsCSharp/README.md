# Lights And Shadows C# — 0.4.2.3

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

## Night shadow polish 0.4.2.2

A intensidade noturna dos raios agora acompanha `uSunStrength * uSunPower` e usa fator 0.24, preservando os 20 samples e as gradientes originais. O alongamento noturno foi suavizado (0.006 por sample), com maior suavidade no limiar da mascara (0.015). No shader de composicao, um piso de 22% da luz lunar e mantido somente no periodo noturno e quando a luz de superficie esta habilitada; isso evita sombras totalmente opacas sem desligar o efeito. O primeiro Kawase com distancia zero usa uma unica leitura exata da mesma amostra, mantendo os 4 passes e alvos. Requer teste Android nas diferentes fases da Lua e em combate; nenhuma melhoria de FPS foi medida ainda.

## Natural Moonlight 0.4.2.3

A intensidade das sombras noturnas passa de 0.24 para 0.15, com raios mais curtos (0.006 para 0.0045), atenuacao pela distancia ao foco lunar e limiar suave 0.024. A cor lunar ganha um preenchimento muito discreto, sem alterar a composicao diurna. Os nove passes, quatro alvos e imagens originais estao mantidos. Testar FPS e biomas no Xiaomi com .88.3.
