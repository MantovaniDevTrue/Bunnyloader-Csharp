# Lights And Shadows — Bunny C# Native

Port experimental do mod Lights de yiyang233/AWEoghAwo, com fidelidade baseada diretamente em Lights-master.zip e nos arquivos Effects/Light.fx, Shadow.fx, Bloom1.fx e KawaseBlur.fx.

A API BunnyScreenEffect.PipelineFile aponta para um arquivo .screenfx. O host nativo cria targets, registra os GLSL, amostra Main.dayTime, Main.time, Main.worldSurface e Main.screenPosition e alimenta os uniforms @sun, @clock, @day, @strength, @threshold e @enabled. Nao utiliza JavaScript.

A segunda etapa contem mascara azul (Shadow.fx:GetShadow) calculada uma unica vez a 1/3 da resolucao, raios de sombra com 30 amostras dia e 20 noite sobre essa mascara, e iluminacao solar/lunar ligada a camera real do Terraria (screenPosition static getter + viewport do renderizador GL). Mantem as gradientes originais ColorTex.png e ColorTex2.png, bloom com extracao e dois passes Kawase (1/3). A opcao ColorTex_Alt.png tambem fica incluida para lote futuro.

Ainda faltam modulacao completa de cor do ceu/bioma, gravidade invertida e configuracoes do PC. O novo mask-pass preserva o calculo pow(1-blue,0.75) do Shadow.fx sem recalcula-lo 30x por pixel. Os shaders PC e o compilador de efeitos tML nao sao copiados para execucao; apenas as formulas e as texturas originais fundamentam este port. Nenhuma alteracao no tModLoader e usada como codigo de execucao.

Empacotar: python3 package_mod.py [caminho-do-bmod]. O script decodifica assets originais de SourceAssets/*.b64 para content/Assets/*.png, sem gerar novos recursos. Exige API Bunny .88 ou posterior. Testes Android e FPS necessarios antes de mesclar.
