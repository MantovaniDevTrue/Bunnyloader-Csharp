# .96 — defaults nativos tipados e armas básicas

Base preservada: `.95.2`, branch `feat/csharp-summon-lifecycle-v95`. APK `.90.29.96-native-defaults-holdouts`, code273; ExampleMod0.1.55. A branch mantém todo o histórico anterior. Sem shaders novos ou dependência de QuickJS/tModLoader.

## Referência e alcance

O novo anexo é ExampleMod1.8.0, SHA256 `c4273f69b27cc32478f3cae3261edec9100189714bb6500cc92d5c78a7867353`, com178 scripts. Todos estão incluídos na referência canônica1.9.0 com206 scripts. Sete scripts diferem; nenhum dos quatro ports deste lote está entre eles. Inventário por arquivo em `Split/Bunny-Loader/tools/example_completion/attachment-reference-1.8.0.json`. A matriz continua com206 entradas, sem excluir os exemplos mais recentes.

Quatro ports: ExampleMeleeWeapon, ExampleBullet, ExampleBulletProjectile e ExampleRocketLauncher. Contêm propriedades, assets e nomes en-US/pt-BR originais. Registrados como implementados aguardando validação Android; a compilação não certifica combate, munição ou desenho no aparelho. ExampleBulletLauncher é uma demonstração adicional para testar ExampleBullet com arma do próprio ExampleMod, não uma substituição de ExampleGun. ExampleGun continua pendente, incluindo áudio personalizado.

## APIs

BunnyItem.BuyPrice/SellPrice convertem moedas como o Terraria: sellPrice retorna cinco vezes o preço de venda desejado para Item.value. Usados nos novos ports; testes verificam espada50000 e bala5000, moedas negativas e overflow.

- `BunnyNativeDefaults`, acessível por NativeDefaults em BunnyItem/BunnyWeapon/BunnyAccessory/BunnyBuffPotion/BunnyProjectile. SetInt/SetFloat/SetBool publicam defaults de campos primitivos mutáveis; SetId resolve constantes ItemID/ProjectileID/AmmoID/ItemUseStyleID/ItemRarityID; SetProjectile resolve nome local ou UID/Nome no registro C++ compartilhado. A referência precisa estar registrada; dependências entre mods devem declarar ordem no manifest. Dependência ausente rejeita o lote com chave e contexto, sem ID inventado.
- O binder valida tipo real, mutabilidade, tamanho1/2/4, limites da instância e representabilidade no inteiro nativo. Escreve somente a largura real via memcpy. Geometria, contagem de atualizações e pilhas têm limites explícitos; NaN, referências nativas, campos estruturais e IDs de projétil não resolvidos são recusados. Até64 campos por conteúdo, descritor16KiB, snapshot imutável publicado após validação do lote. Não faz buscas de metadata nem aloca descritores por frame.
- Usar propriedades normais do SDK para ciclo de vida, registros e políticas; NativeDefaults acrescenta defaults primitivos, não instala comportamento gerenciado por alterar uma flag isolada. Também não é API para escrever entidades a cada frame. Métodos/propriedades nativas, objetos, arrays, campos não listados e chamadas arbitrárias não são expostos por este serviço. Uma API faltante continua pendência, não limitação comprovada do Android.
- `BunnyHoldout` opcional em BunnyItem/BunnyWeapon: hooks nativos UseStyle/HoldStyle aplicam posição relativa com direção, gravidade e rotação. Valida campos do Player, assinaturas completas e Rectangle de16bytes, incluindo convenção de offsets boxed/raw. Não exige callbacks Mono por frame.
- `[BunnyName(culture,text)]` registra nomes localizados no mesmo serviço nativo de conteúdo usado pelo JS, sem executar a VM. Extração antes do registro preserva nomes durante a instalação das tabelas. Idiomas únicos, até8, nomes UTF-8 até128bytes. Usado nos quatro novos ports.

O protocolo mantém as linhas V2 existentes e adiciona NATIVE_ITEM/NATIVE_PROJECTILE/HOLDOUT. Cache compilador v28 invalida resultados incompatíveis. NativeContentSpec não foi modificado. A API e as fontes antigas permanecem disponíveis; nomes/UIDs persistidos não mudam.

## Diagnóstico de receitas

A captura mostra Kind0/WaitRecipeCallback/DeadlineExceeded com budget2997us. A fila de receitas estava atrás da fila de entidades no worker. O processador agora vem antes da IA, mantém o limite3ms e o cancelamento de trabalhos vencidos, e nunca executa callback cancelado. O fixture executa o processador real, desbloqueio durante chamada, resposta, cancelamento e prazo. Isso reduz contenção; não promete eliminar deadlines de métodos C# arbitrariamente lentos ou inicialização fria. Ainda falta medição no Android; não removemos o aviso nem transformamos timeout em sucesso.

## Validação e continuidade

Fixtures executam binder/publicadores/hooks reais com metadata e funções IL2CPP controladas, sem QuickJS. Cobrem bytes sentinela ao lado de Byte, IDs simbólicos, referência de outro mod, ausência/tipo/storage/offset inválido, publicação transacional, descritores do pacote compilado, holdout com direção/gravidade/rotação, isolamento vanilla, localização e finalização de escala/maxPenetrate antes do combate. Não executam o motor Terraria. O fixture de transporte é controlado; não simula certificação de JIT/latência no telefone.

A captura posterior à entrega .95.2 mostra `Killed Reason=BuffCancelled` e baseline30/incoming36/item30. O usuário informou que funcionou; a imagem não exibe versão/PID. Registrar evidência parcial de cancelamento e origem do dano, sem promover o minion inteiro nem afirmar FPS.

Próximos grupos: mecanismos de disparo/ModifyShootStats/Shoot, consumo mana/ammo e sons customizados para ExampleGun/Shotgun; IA/draw completos da magia animada; demais famílias de armas/projéteis. Seguem abertos todos os outros lotes e os206 exemplos. O loader completo não está concluído.

CI39186174513 bloqueou a entrega no smoke independente que ainda exigia48 classes e53 linhas anteriores. O pacote novo produz53 classes e64 linhas; o smoke foi atualizado e ampliado para verificar preços, munição simbólica e offsets reais dos novos exemplos. Execução completa local do mesmo smoke passou, incluindo recompilação por edição e segundo mod independente. Nenhum artefato dessa tentativa foi publicado.

### Próximo grupo, sem recriar o que já existe

- `Content/Items/Weapons/Ranged/ExampleGun.js`: defaults, munição e offset(-18,0) já são expressáveis; falta áudio de `Sounds/Items/Guns/ExampleGun` com volume0.9, variação de pitch0.2 e limite3. Investigar `script/api/Sounds.cpp` e o serviço nativo chamado por esse adaptador antes de criar outro motor de áudio. A bridge C# deve chamar o serviço diretamente, sem construir SoundStyle JavaScript.
- `Content/Items/Weapons/Ranged/ExampleShotgun.js`: defaults/offset(-2,-2) desbloqueados; falta Shoot gerenciado reutilizável com oito disparos, ângulo aleatório±15°, velocidade multiplicada por0.7..1 e supressão do disparo original. Referência Android: `Player.ItemCheck_Shoot(int,Item,int,bool)` e `Projectile.NewProjectile(IEntitySource,float,float,float,float,int,int,float,int,float,float,float,NewProjectileModifier)` em `script/js/mod/Loaders/ItemLoader.js`. Preservar fonte, dono, dano já modificado e munição; isolar contexto aninhado para não interceptar os oito spawns como outro uso da arma. Testar zero/um/oito spawns e coexistência com hooks JS.
- `Content/Items/Weapons/Magic/ExampleMagicWeapon.js`: flags/mana/shootSpeed e som vanilla são expressáveis; depende do port integral de `ExampleAdvancedAnimatedProjectile`, incluindo sua IA/animação, e do equivalente real de SetDefaultWeaponStyle. Não registrar arma apontando para projétil inexistente.
- `Content/Items/Weapons/Magic/ExampleLaserWeapon.js`: falta clone seguro dos defaults de LastPrism, CanUseItem condicionado à contagem de projéteis do dono e os ports completos de holdout/laser. O offset visual BunnyHoldout não equivale ao projétil ExampleLaserHoldout. Não marcar como implementado por compartilhar o nome holdout.

As ausências acima são trabalho de API ainda não concluído; não constituem limitações comprovadas do Terraria Android. Para seguir, usar a matriz de206 entradas e o inventário por hash do anexo, manter os IDs atuais e acrescentar testes de execução ao lado dos fixtures existentes.

## Entrega experimental verificada — 19/10/2026

Fonte testada `96c90e6343bad4d0eed42724e6b777565f864eec`. [Android CI39186732013](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39186732013) e suas nove suítes independentes no mesmo SHA passaram. Compilação Android, assinatura V3, publicação privada e job de conferência da entrega aprovados. [APK .96 e ExampleMod0.1.55](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp-Builds/releases/tag/csharp-v3-run-39186732013-a1).

O `.bmod` publicado tem76 fontes editáveis e47 PNGs, todos comparados byte a byte com o repositório. O compilador emite53 referências de assets (45 caminhos distintos, com extensões opcionais suportadas). SHA256 do pacote `fb41ae91478a73cf137d699faf31558d0c3d1a9ce32ed1c830016cda968dd5d0`; matriz com206 entradas e guia novo conferidos. Não contém DLLs, bibliotecas nativas ou scripts JS.

35 testes gerenciados de itens,43 verificações de entidades,31 casos de combate, oito executáveis nativos e smoke Roslyn completo passaram. Novos fixtures de defaults/holdout passaram ASan/UBSan no host. [Registro completo, percentuais e checksums](validation/csharp-v96-ci-2026-10-19.json). O commit seguinte é documental e não altera os artefatos testados. Os quatro ports novos seguem aguardando Android; o port integral dos206 exemplos e os88 loaders continuam em andamento.
