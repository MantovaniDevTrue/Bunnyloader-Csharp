# Exemplos nativos — .96 / ExampleMod0.1.55

## Como funciona

BunnyItem continua sendo uma classe C# normal: pode herdar, chamar métodos auxiliares, calcular valores e utilizar loops em SetDefaults. NativeDefaults envia valores prontos ao host; ele encontra o campo real e valida seu tipo/tamanho antes de escrever. Não passe offsets nem IDs de conteúdo de mod gravados em saves.

`NativeDefaults.SetBool("ranged", true).SetId("ammo", "AmmoID", "Bullet").SetProjectile("shoot", nameof(ExampleBulletProjectile))` configura munição nativa. Os nomes simbólicos evitam fixar números que podem variar. Referências UID/Nome de outro mod usam o registro C++ compartilhado; declare a dependência e sua ordem no manifest. Dependência ausente produz erro específico, não fallback para conteúdo errado.

SetInt respeita Byte/Int16/Int32 reais; SetFloat aceita Single finito; SetBool exige Boolean de1byte. Só defaults primitivos conhecidos são permitidos; identidade, dono, atividade, ponteiros, arrays, propriedades/métodos e campos não suportados são recusados. A API não é acesso arbitrário à memória nem executa alterações de entidade a cada frame. Use as propriedades e hooks do SDK para suas políticas e ciclo de vida.

BunnyHoldout recebe offsets no referencial local da arma; o núcleo aplica direção, gravidade e rotação depois dos estilos nativos de uso e descanso. BunnyName registra nomes por cultura antes da criação das tabelas.

## Teste no Terraria

1. Mantenha ExampleMod habilitado. Pegue **Example Espada** (ExampleMeleeWeapon), **Example Bala** (ExampleBullet), **Example Bullet Launcher** e **Example Lança-Foguetes** (ExampleRocketLauncher) no catálogo do mod. Não são IDs numéricos fixos.
2. Gere **Example Slime** do ExampleMod. Ataque com a espada: conferir dano, crítico, repulsão, swing/uso automático e som. Dica esperada: dano50, crítico6, uso20; modificadores do personagem/prefixos podem alterar os números finais.
3. Coloque somente ExampleBullet nos slots de munição, equipe ExampleBulletLauncher e dispare. O jogo deve selecionar/consumir a bala, usar seu sprite, velocidade e dano somados à arma. Dano crítico/modificadores são nativos. Confira pilha antes/depois.
4. Gere mais de um Example Slime alinhado. A bala tem penetração5, imunidade local-1, luz0.5 e uma subatualização extra. Conferir colisão com blocos/água, luz e ausência de acertos repetidos contra o mesmo NPC com a mesma bala. O teste host não certifica a colisão do motor.
5. Para ExampleRocketLauncher, use munição Rocket vanilla, como exige o original. O objeto sob teste é o lançador C# do ExampleMod. Conferir consumo, explosão nativa, somItem11 e posição da arma ao virar/atirar; a arma deve seguir a mira e o offset(-2,-2).
6. Alterne pt-BR/en-US nas configurações. Example Espada/Example Sword e nomes da bala/lançador devem corresponder ao idioma. Salve, saia, reentre e confira pilhas, itens e prefixos preservados.
7. Refaça o teste do minion/bancada existente. Observe se o aviso RecipeCallback diminuiu durante crafting com combate; anote relatório/PID e momento. A mudança de prioridade não garante prazo para callbacks arbitrariamente caros.

ExampleBulletLauncher é um exemplo adicional do SDK, não o port de ExampleGun: o port separado de ExampleGun foi acrescentado na continuidade0.1.56, com áudio próprio documentado em CUSTOM_AUDIO.md e validação Android pendente. Estes passos validam os novos recursos no aparelho; compilação/CI não substituem esse teste nem garantem60FPS. Nenhuma categoria da matriz foi removida.

## Valores monetários

Use BunnyItem.BuyPrice/SellPrice, inclusive com argumentos nomeados. Item.value guarda o valor-base de compra: SellPrice(silver:10) retorna5000, para vender por1000cobres. Os helpers recusam moedas negativas e overflow; não escreva simplesmente1000 em Value para esse preço de venda.

Itens colocáveis continuam usando DefaultToPlaceableTile; o descriptor de defaults não substitui os serviços de colocação, buffs, spawn ou ciclo de vida. Categorias de munição numéricas são vanilla; referências de projétil de mod usam SetProjectile por nome.

Na continuidade0.1.57, nomes exibidos/traduções/prefixos começam com Example por solicitação do usuário. IDs e assets permanecem estáveis. Disparos personalizados: SHOOTING.md.
