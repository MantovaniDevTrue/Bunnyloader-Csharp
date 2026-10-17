# Lights And Shadows — Bunny C# Native

Port experimental do mod Lights de yiyang233/AWEoghAwo, com fidelidade baseada diretamente em Lights-master.zip e nos arquivos Effects/Light.fx, Shadow.fx, Bloom1.fx e KawaseBlur.fx.

A API BunnyScreenEffect.PipelineFile aponta para um arquivo .screenfx. O host nativo cria targets, registra os GLSL, amostra Main.dayTime, Main.time, Main.worldSurface e Main.screenPosition e alimenta os uniforms @sun, @clock, @day, @strength, @threshold e @enabled. Nao utiliza JavaScript.

A primeira etapa contem raios de sombra a 1/3 da resolucao (30 amostras dia, 20 noite), iluminacao solar/lunar usando as gradientes originais ColorTex.png e ColorTex2.png, bloom com extracao e dois passes Kawase (1/3). A opcao ColorTex_Alt.png tambem fica incluida para lote futuro.

As formulas para cor do ceu, biome, gravidade invertida e moon-phase foram parcialmente aproximadas no primeiro lote; nao equivale a fidelidade completa do mod de PC. Nenhuma alteracao no tModLoader e usada como codigo de execucao.

Empacotar: python3 package_mod.py [caminho-do-bmod]. O script decodifica assets originais de SourceAssets/*.b64 para content/Assets/*.png, sem gerar novos recursos. Exige API Bunny .87 ou posterior. Testes Android e FPS necessarios antes de mesclar.
