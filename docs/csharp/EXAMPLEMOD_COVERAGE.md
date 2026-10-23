# ExampleMod CSharp — cobertura rastreável

Gerado por `python3 tools/examplemod_coverage.py`. Fontes e evidências em `reference/` e `examplemod-evidence.json`.

**Trabalho em andamento. Não há paridade completa nem validação Android deste lote.**

Base: `fcdc3fd2c30d1e7432fddbafc4af9f6d65ca473a`, Bunny **1.0.11 Dust/Invasion QA**. Os 174 arquivos anexados eram idênticos à base; 98 fontes C# preservados.

FPS, memória e tempos no aparelho continuam `null`: não foram medidos. Relatos de versões antigas não validam esta versão.

O catálogo usa cada hook JS como unidade inicial de revisão, com hash do fonte. Os efeitos internos ainda precisam ser detalhados por lote. Coincidência de nome só sugere um candidato C#; não promove status. O inventário oficial tML inclui arquivos antigos, que exigem triagem de pertinência.

| Estado | Comportamentos |
|---|---:|
| não iniciado | 687 |
| implementado sem validar | 1 |
| validado automaticamente | 34 |
| validado no Android | 1 |
| falhou | 3 |
| bloqueio comprovado | 0 |

Inventários: 100 fontes C#, 206 fontes JS e 558 fontes oficiais tML. Esses números não medem conclusão.

Dependência encontrada: tooltips C# fornecem um plano nativo, mas `Exports.js` instala `TooltipLoader.js` para executar o comportamento. A independência completa de JS está pendente.

Dust e invasão: o baseline contém correções posteriores aos logs Android de falha de contrato/already-running; nenhuma nova validação em aparelho foi produzida aqui.

| Comportamento | Estado | Observação |
|---|---|---|
| Content/Items/Accessories/WaspNest.js#SetDefaults | validado automaticamente | Valores originais (26x26, Green=2, sellPrice 2 gold) e strongBees funcional; Back autoload do JS tem política C++ equivalente. Testes nativos sem QuickJS cobrem slots/desenho, equipamento/vaidade e reset; Android pendente. |
| Content/Items/Accessories/WaspNest.js#UpdateAccessory | validado automaticamente | Valores originais (26x26, Green=2, sellPrice 2 gold) e strongBees funcional; Back autoload do JS tem política C++ equivalente. Testes nativos sem QuickJS cobrem slots/desenho, equipamento/vaidade e reset; Android pendente. |
| Content/Items/Armor/ExampleBreastplate.js#AddRecipes | validado automaticamente | Valores, receitas e textos originais das peças conferidos no protocolo Roslyn; efeitos de peça e os quatro bônus do conjunto executados nos hooks C++ sem QuickJS. A localização do texto de conjunto é uma pendência separada em AddArmorSets. Aparência e execução Android pendentes. |
| Content/Items/Armor/ExampleBreastplate.js#ModifyTooltipLines | validado automaticamente | Valores, receitas e textos originais das peças conferidos no protocolo Roslyn; efeitos de peça e os quatro bônus do conjunto executados nos hooks C++ sem QuickJS. A localização do texto de conjunto é uma pendência separada em AddArmorSets. Aparência e execução Android pendentes. |
| Content/Items/Armor/ExampleBreastplate.js#SetDefaults | validado automaticamente | Valores, receitas e textos originais das peças conferidos no protocolo Roslyn; efeitos de peça e os quatro bônus do conjunto executados nos hooks C++ sem QuickJS. A localização do texto de conjunto é uma pendência separada em AddArmorSets. Aparência e execução Android pendentes. |
| Content/Items/Armor/ExampleBreastplate.js#UpdateEquip | validado automaticamente | Valores, receitas e textos originais das peças conferidos no protocolo Roslyn; efeitos de peça e os quatro bônus do conjunto executados nos hooks C++ sem QuickJS. A localização do texto de conjunto é uma pendência separada em AddArmorSets. Aparência e execução Android pendentes. |
| Content/Items/Armor/ExampleHelmet.js#AddArmorSets | implementado sem validar | Registro nativo e texto inglês do conjunto passam no host. Falta atualizar o bônus com a tradução original ao trocar de idioma; sem equivalência completa nem validação Android. |
| Content/Items/Armor/ExampleHelmet.js#AddRecipes | validado automaticamente | Valores, receitas e textos originais das peças conferidos no protocolo Roslyn; efeitos de peça e os quatro bônus do conjunto executados nos hooks C++ sem QuickJS. A localização do texto de conjunto é uma pendência separada em AddArmorSets. Aparência e execução Android pendentes. |
| Content/Items/Armor/ExampleHelmet.js#SetDefaults | validado automaticamente | Valores, receitas e textos originais das peças conferidos no protocolo Roslyn; efeitos de peça e os quatro bônus do conjunto executados nos hooks C++ sem QuickJS. A localização do texto de conjunto é uma pendência separada em AddArmorSets. Aparência e execução Android pendentes. |
| Content/Items/Armor/ExampleHelmet.js#UpdateArmorSet | validado automaticamente | Valores, receitas e textos originais das peças conferidos no protocolo Roslyn; efeitos de peça e os quatro bônus do conjunto executados nos hooks C++ sem QuickJS. A localização do texto de conjunto é uma pendência separada em AddArmorSets. Aparência e execução Android pendentes. |
| Content/Items/Armor/ExampleLeggings.js#AddRecipes | validado automaticamente | Valores, receitas e textos originais das peças conferidos no protocolo Roslyn; efeitos de peça e os quatro bônus do conjunto executados nos hooks C++ sem QuickJS. A localização do texto de conjunto é uma pendência separada em AddArmorSets. Aparência e execução Android pendentes. |
| Content/Items/Armor/ExampleLeggings.js#ModifyTooltipLines | validado automaticamente | Valores, receitas e textos originais das peças conferidos no protocolo Roslyn; efeitos de peça e os quatro bônus do conjunto executados nos hooks C++ sem QuickJS. A localização do texto de conjunto é uma pendência separada em AddArmorSets. Aparência e execução Android pendentes. |
| Content/Items/Armor/ExampleLeggings.js#SetDefaults | validado automaticamente | Valores, receitas e textos originais das peças conferidos no protocolo Roslyn; efeitos de peça e os quatro bônus do conjunto executados nos hooks C++ sem QuickJS. A localização do texto de conjunto é uma pendência separada em AddArmorSets. Aparência e execução Android pendentes. |
| Content/Items/Armor/ExampleLeggings.js#UpdateEquip | validado automaticamente | Valores, receitas e textos originais das peças conferidos no protocolo Roslyn; efeitos de peça e os quatro bônus do conjunto executados nos hooks C++ sem QuickJS. A localização do texto de conjunto é uma pendência separada em AddArmorSets. Aparência e execução Android pendentes. |
| Content/Items/ExampleGolfBall.js#SetDefaults | validado automaticamente | Tradução dos valores originais, assets e nomes en-US/pt-BR. Compilação Roslyn do mod real e execução do hook C++ de produção sem QuickJS verificam as tabelas antes do SetDefaults original. Física, renderização e rede no Android ainda não foram validadas. |
| Content/Items/Materials/ExampleSoul.js#GetAlpha | validado automaticamente | Protocolo Roslyn dos fontes reais e hooks C++ sem QuickJS verificam clone, valores, animação 4/6, três tabelas, RGBA e iluminação RGB no centro com essScale; Android e aparência pendentes. |
| Content/Items/Materials/ExampleSoul.js#PostUpdate | validado automaticamente | Protocolo Roslyn dos fontes reais e hooks C++ sem QuickJS verificam clone, valores, animação 4/6, três tabelas, RGBA e iluminação RGB no centro com essScale; Android e aparência pendentes. |
| Content/Items/Materials/ExampleSoul.js#SetDefaults | validado automaticamente | Protocolo Roslyn dos fontes reais e hooks C++ sem QuickJS verificam clone, valores, animação 4/6, três tabelas, RGBA e iluminação RGB no centro com essScale; Android e aparência pendentes. |
| Content/Items/Materials/ExampleSoul.js#SetStaticDefaults | validado automaticamente | Protocolo Roslyn dos fontes reais e hooks C++ sem QuickJS verificam clone, valores, animação 4/6, três tabelas, RGBA e iluminação RGB no centro com essScale; Android e aparência pendentes. |
| Content/Items/Tools/ExampleHamaxe.js#AddRecipes | validado automaticamente | Valores e receita conferidos no C# real: Green=2, poderes/tempos/dano/knockback originais e 10 ExampleItem na Work Bench. Uso/mineração no Android pendentes. |
| Content/Items/Tools/ExampleHamaxe.js#SetDefaults | validado automaticamente | Valores e receita conferidos no C# real: Green=2, poderes/tempos/dano/knockback originais e 10 ExampleItem na Work Bench. Uso/mineração no Android pendentes. |
| Content/Items/Tools/ExamplePickaxe.js#AddRecipes | validado automaticamente | Valores e receita conferidos no C# real: Green=2, poderes/tempos/dano/knockback originais e 10 ExampleItem na Work Bench. Uso/mineração no Android pendentes. |
| Content/Items/Tools/ExamplePickaxe.js#SetDefaults | validado automaticamente | Valores e receita conferidos no C# real: Green=2, poderes/tempos/dano/knockback originais e 10 ExampleItem na Work Bench. Uso/mineração no Android pendentes. |
| Content/Projectiles/ExampleGolfBallProjectile.js#SetDefaults | validado automaticamente | Tradução dos valores originais, assets e nomes en-US/pt-BR. Compilação Roslyn do mod real e execução do hook C++ de produção sem QuickJS verificam as tabelas antes do SetDefaults original. Física, renderização e rede no Android ainda não foram validadas. |
| Content/Projectiles/ExampleGolfBallProjectile.js#SetStaticDefaults | validado automaticamente | Tradução dos valores originais, assets e nomes en-US/pt-BR. Compilação Roslyn do mod real e execução do hook C++ de produção sem QuickJS verificam as tabelas antes do SetDefaults original. Física, renderização e rede no Android ainda não foram validadas. |
| requirement:tooltip-without-js | falhou | Dependência confirmada por inspeção de Exports.js/TooltipLoader.js; substituir por renderer nativo. |
| requirement:aim-before-shoot | validado automaticamente | Falha myPlayer do APK 311 corrigida no 312; usuário relatou armas funcionando. No lote 313, regressões de mira passam no host; nova opção do cajado ainda aguarda Android. O resolvedor agora aceita getter estático válido mesmo quando há um campo de instância homônimo; a ausência de ambos continua rejeitada. |
| requirement:ExampleGun | validado automaticamente | Asserções dos valores/algoritmo do exemplo real e protocolo compilado no host. Trajetória, cano e consumo no motor Android pendentes. |
| requirement:ExampleRocketLauncher | validado automaticamente | Asserções dos valores/algoritmo do exemplo real e protocolo compilado no host. Trajetória, cano e consumo no motor Android pendentes. |
| requirement:ExampleShotgun | validado automaticamente | Asserções dos valores/algoritmo do exemplo real e protocolo compilado no host. Trajetória, cano e consumo no motor Android pendentes. |
| requirement:dust-emission-binding | validado automaticamente | Corrigido e executado no host; reteste Android do novo lote pendente. Ver SUMMON_DUST_INVASION_QA.md. |
| requirement:android-weapons-user-report | validado no Android | Usuário: As armas funcionou. Escopo: relato de funcionamento; contagem detalhada de pellets, multiplayer e FPS não informados. |
| requirement:summon-staff-reference | validado automaticamente | Relato após 314 chegou ao binding e falhou em ItemRarityID.Cyan: classe do tModLoader ausente no Android. Correção em fontes implementa os 17 nomes em C++; teste usa todo o descritor compilado do cajado e reproduz a falha com o resolvedor anterior. Host aprovado; Novo APK bloqueado até concluir os loaders; Android pendente. |
| requirement:android-dust-emission | falhou | Relatório 0.1.64: NewDustSignatureUnavailable. Correção do namespace Graphics.Color no novo lote ainda não confirmada em aparelho. |
| requirement:android-invasion-spawn | falhou | Running com SpawnFailed crescente e NoSafeSpawnSite; busca/diagnósticos ajustados, reteste pendente. |
| requirement:apk-after-loaders | validado automaticamente | Instrução do usuário em 2026-10-23. Validador executado: bloqueia o estado atual. Gradle e workflow configurados para chamar o validador; configuração Gradle não executada neste host. |
| requirement:angler-static-properties | validado automaticamente | Inspeção confirma getters estáticos e setter de npcChatCornerItem. Teste executa loader C++ real: propriedades com campos de instância homônimos, campos estáticos, tipos inválidos, exceções, seleção/chat e unload. Android pendente; não declara o loader inteiro concluído. |
| requirement:rarity-mouse-text-cache-android-layout | validado automaticamente | Metadados do Android 301720 identificam estrutura mista de 48 bytes, com diff Byte e referências String. O binder normaliza offsets crus/boxed e aceita passagem indireta somente no ARM64. O tooltip identifica a classe aninhada por identidade, sem presumir namespace Terraria. Fixture executa scopes e forwarding reais sem QuickJS; não valida renderização visual, instalação dos hooks no aparelho nem o loader inteiro. |
| requirement:projectile-static-defaults-lifecycle | validado automaticamente | O compilador executa o hook após SetDefaults do protótipo, rejeita new em vez de override e propaga exceções. Tabelas são aplicadas na thread do Terraria antes do spawn e antes do SetDefaults original. Retirada seletiva, campos removidos, recarga imediata, consumidores posteriores e 1000 publicações/retiradas concorrentes são exercitados pelo fixture nativo. A alteração cobre essas políticas; não encerra o lifecycle global de todos os loaders. |

Todos os hooks e o backlog oficial estão em [EXAMPLEMOD_COVERAGE.json](EXAMPLEMOD_COVERAGE.json).

Próximos lotes: tooltip nativo independente de JS; confirmação Android Dust/invasão; paridade de NPC/bosses/projéteis; tiles/entidades/persistência; players/globals/config/UI; multiplayer e integração/unload. Nenhum desses lotes é declarado completo por inventário ou compilação.
