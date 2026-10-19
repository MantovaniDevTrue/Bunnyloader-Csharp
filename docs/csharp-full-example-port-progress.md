# Port integral — desenvolvimento sem entregas parciais

Regra vigente de19/10/2026: não publicar novo APK ou .bmod enquanto o port completo estiver pendente. A entrega .96 é histórica. A branch atual preserva seus commits; políticas de publicação push/PR foram removidas para essa branch, o publicador também recusa invocação direta e o export de pacotes depende da mesma política. CI, compilação e relatórios técnicos continuam disponíveis. Não reabilitar publicação para entregar outro subconjunto.

## Estado de implementação

Referência canônica:206 JS. O anexo1.8.0 tem178, todos contidos na referência1.9.0. Nenhum foi removido da matriz. Revisão de desenvolvimento `.90.29.97-full-example-port-wip`/code274, ExampleMod0.1.56,77 fontes C# editáveis, SDK89 tipos públicos. Não é release, não conclui os lotes e não foi validada no telefone.

### Áudio nativo reutilizável

`BunnySoundStyle` configura um asset WAV/Ogg/MP3 relativo a content/, volume, pitch, variação, máximo de instâncias e IgnoreNew/ReplaceOldest. `BunnyItem.CustomUseSound` e `BunnyWeapon.CustomUseSound` têm precedência sobre som vanilla. O SDK valida caminhos/valores; o protocolo auxiliar USE_SOUND preserva linhas primárias V2. Cache compilador v29 invalida resultados anteriores.

O SourceLoader resolve dono pelo registro nativo e transfere o descritor ao serviço `NativeUseSounds`. Este valida diretório/arquivo real, confinamento após resolver symlinks, tamanho do asset, campo mutável `Item.UseSound` de referência LegacySoundStyle, assinatura completa do construtor e SoundType enum de4bytes. Cria marker1000 com style registrado no serviço de áudio comum e mantém handle GC forte. A publicação da tabela é atômica, depois de todos os markers; falhas liberam raízes e estilos temporários. A escrita usa barreira do GC, ou fallback somente se o GC foi confirmado não incremental. Não chama QuickJS nem escreve offsets fornecidos pelo mod.

O roteador compartilhado agora usa SfxQueue, com fila/histórico limitados, em vez de manter uma segunda fila C++ e mapas de streams/tokens sem descarte natural. Preserva os tokens JS na faixa antiga, limites, substituição, pausa e cancelamento. Campos e getters estáticos de áudio são validados antes da leitura; instalação rejeitada retorna erro, não style falsamente utilizável. Liberação de estilo não desloca IDs e não reutiliza placeholders JS de som inválido.

ExampleGun possui textura e Ogg originais, ranged/ammoBullet/shootSpeed10/fallbackPurificationPowder, dano20/knockback5/crit0, uso10/autoReuse/noMelee, sellPrice10gold e offset(-18,0). Usa volume0.9, variação0.2 e máximo3. Não substitui ExampleBulletLauncher nem outros fontes existentes. Está implementado aguardando validação, sem certificação de áudio/combate Android.

## Testes

-36 casos gerenciados de itens/receitas, incluindo validação do áudio e compilação/execução do pacote inteiro.
-43 verificações de entidades e31 casos de combate preservados.
-Dez executáveis nativos: os oito anteriores, decode/binder/publicador/raízes/barreira/rollback com wire do ExampleGun compilado e roteador C++ real com campos/getters inválidos, compatibilidade de tokens JS, limites, pausa, recusa e dez mil conclusões.
-Smoke Roslyn completo:54 classes/68 linhas, fontes editáveis, recompilação e segundo mod independente.
-Verificação do pacote exige também Ogg original e recusa áudio ausente/cabeçalho inválido; cabeçalho não prova decodificação no Android.
-Contratos IL2CPP/receitas/projéteis e sintaxe dos fontes nativos modificados preservados. Sanitizers e CI devem ser registrados separadamente depois da execução.

Os providers IL2CPP/Android dos testes de host são controlados. Não executam SoundPool no telefone, decodificador Android nem combate Terraria. Gameplay/FPS e compilação fria em5s não foram medidos nesta revisão. Não há pacote novo entregue ao usuário.

## Próximo sistema compartilhado

Implementar Shoot/ModifyShootStats gerenciados, com snapshot tipado e lista de spawns validada. Consultar `script/js/mod/Loaders/ItemLoader.js`, contratos reais de `Player.ItemCheck_Shoot(int,Item,int,bool)` e `Projectile.NewProjectile(IEntitySource,float,float,float,float,int,int,float,int,float,float,float,NewProjectileModifier)`. Preservar fonte/dono/dano já modificado/munição e direção de mira. Não usar um helper fixo de oito tiros como substituto do callback original.

Reutilizar transporte do worker Mono com identificação, limite de fila, prazo e cancelamento, sem misturar métricas de receitas. Preaquecer dispatcher e instâncias para não mascarar inicialização fria com tiro original. Validar todas as saídas antes de tocar o motor, executar spawns na thread do jogo e impedir reentrada dos próprios spawns no mesmo callback. Testar zero/um/oito tiros, exceção/atraso/retorno inválido, ordem de outros hooks JS/C# e dano derivado da arma.

Portar ExampleShotgun e todos os outros exemplos que essa infraestrutura desbloquear. Prosseguir magia/holdouts/IA/draw e demais famílias conforme a matriz; não recriar bancada, armaduras ou summons já existentes e não declarar outros exemplos completos por compartilhar uma propriedade. Restam todos os lotes, até contabilizar e implementar o comportamento de cada arquivo. APIs ainda faltantes são pendências, não limitações técnicas comprovadas.

Configurações de som registradas são imutáveis durante a sessão: repetir a mesma definição reutiliza o registro; mudar asset/opções requer reiniciar o jogo. A recusa explícita evita reutilizar um style ID ainda referenciado por itens antigos. Não é suporte a hot reload de áudio.

Sanitizers dos novos testes de áudio (ASan/UBSan) passaram no host. CI Android ainda deve ser registrado no commit exato; não há validação nova no dispositivo.
