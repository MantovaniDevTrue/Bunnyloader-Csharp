# Integração da última build + políticas próprias de projéteis — .93

## Bases preservadas

Build mais recente examinada: .90.5, fonte
`ae582032b20d989ec5b9be87c7268355feab7531`, run39152341737 aprovado, PR94.
Integração por merge com `3cfffc52b3a560561c75326e596a745916e539f7` (.92/PR93),
sem descartar os commits das duas branches. Commit de merge `e73746d9`.
Branch `feat/csharp-owned-loaders-v93`; Bunny .90.29.93/code268,
ExampleMod0.1.52; Lights0.4.2.3 preservado. O pacote contém71fontes editáveis,48classes e15receitas no wire. A última build .90.5 não incluía
as receitas comuns .92: agora os dois percursos estão na mesma fonte.

## Funcionalidades novas

- Propriedades públicas AnimationFrameTicks, MinionScanTicks e MinionTargetRange,
  com limites e diagnósticos no compilador e no decoder nativo. A extensão V2 de
  projétil tem16campos quando necessária, preservando8/12/13campos anteriores.
  Cache do compilador v26; a identidade do runtime/SDK invalida caches antigos.
- Política cacheada por instância: nenhuma busca de metadata por frame, cadência
  de animação nativa, raio/intervalo real do scanner nativo e alvo entregue ao
  MonoVM. O scanner conserva o limite anterior de200NPCs/1024entradas na tabela.
- BunnyMinionMovement imutável e reutilizável; overload configurável de steering
  com velocidades, inércias e repouso. Corrige velocidade residual quando a
  posição coincide exatamente com o ponto de repouso. Não aloca config por tick.
- Validação nativa de minion/minionSlots/frame/frameCounter antes de uso: tipo,
  tamanho, bounds, storage e campos InitOnly; binding transacional. O scanner
  valida os campos NPC e o tipo/rank da tabela. Ausência de dontTakeDamage é
  opcional; presença com tipo errado desativa a operação com motivo técnico.
  Erros da nova política atravessam SourceLoader sem perder o detalhe original.
- ExampleMinion, ExampleMinionBuff e ExampleMinionItem no ExampleMod principal,
  usando as APIs públicas, assets originais e receita na bancada C#. O probe
  independente anterior continua preservado. IDs persistidos não foram renomeados.

## Testes e alcance da evidência

Tests/basic_items compila fontes reais, verifica protocolo e rejeição de valores
inválidos pelo probe; executa movimento customizado, amortecimento, NaN e limites.
Tests/projectile_policy consome o wire realmente compilado, executa o decoder e
operação de animação de produção com metadata controlada; rejeita tipo/tamanho,
offset/readonly/static inválidos e confirma sentinelas/replay de frames. Testes
anteriores de receitas, callbacks e ciclo de entidades são preservados.

Não foi executado Terraria nesta máquina. Nenhuma afirmação de60FPS, ABI Android
validada ou compilação fria<=5s se baseia nesses testes. O fluxo privado exige
nove regressões no mesmo SHA, compilação Android, assinatura e verificação da
release. Registrar resultado real após o CI.

## Continuidade honesta

A matriz dos206JS e o inventário oficial1221arquivos permanecem completos. Os
três exemplos de summon recebem estado Parcial: faltam LOS/alvo manual,
teleporte/separação/luz/rotação originais, atributos/spawn original do cajado e
contador do buff. Não há bloqueio técnico comprovado para essas lacunas; são
implementação pendente. Receitas não expõem ainda descrições das condições na UI.
Demais loaders continuam no escopo, sem criar classes vazias para inflar cobertura.

Teste Android: guia `PROJECTILE_LOADER_V93.md` incluído no bmod. Use apenas
conteúdo do ExampleMod: bancada, material, cajado/minion/buff e receitas da .92.


## Próximos passos concretos

Preservar o [PR96](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/pull/96)
e este merge ao integrar na linha principal. Validar o guia Android sem o probe
antigo. Seguir pela relação de lacunas na matriz dos três exemplos summon:
campos reutilizáveis do cajado (incluindo mana/som/knockback), spawn na mira e
alvo manual/LOS; depois teleporte, separação, luz e contador do buff. Não afirmar
que mudar o nome de um helper finaliza o ProjectileLoader. O loader de receitas
mantém condicionais/consumo/OnCraft; apresentação das condições e demais contextos
continuam pendentes, sem remover o que já funciona.
