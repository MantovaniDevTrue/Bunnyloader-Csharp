# PrefixLoader C# — validação integrada Android

Estado: roteiro de QA, **não** certificação de gameplay. O CI de C#/C++ e a compilação ARM64 não substituem o Terraria 1.4.5.8.6 no aparelho.

## Pré-condições e segurança

- Usar somente uma build de teste **assinada e autorizada** do Bunny Loader, gerada pelo pipeline privado existente, depois que os respectivos gates permitirem o APK. Não distribuir assets/assemblies do Terraria em release pública nem contornar o gate de cinco loaders.
- Registrar commit exato do APK, versão do Bunny Loader, Android, dispositivo, hash da build e mods ativos.
- Primeiro testar o ExampleModCSharp isolado. Depois repetir com um mod de terceiros que registre prefixos, anotando a identidade e a versão.
- Desativar ferramentas que alterem artificialmente FPS e usar o mesmo mundo, resolução e qualidade gráfica entre medidas.
- O `bl.csharp.nativePrefixDiagnostics()` do **diagnóstico interno** retorna contadores monotônicos por processo; não é uma API de callbacks C# para mods. O hook mede tempo acumulado e máximo de `RollAPrefix` em microssegundos, incluindo o fallback nativo. Não mede FPS.

## Coleta de diagnóstico

Antes e depois de cada série de testes, capturar o resultado do diagnóstico interno `nativePrefixDiagnostics()` e calcular a diferença de seus campos: `rollAttempts`, `restoredRolls`, `chosenItemRolls`, `preferredRolls`, `weightedRolls`, `vanillaFallbacks`, `rejectedRolls`, `totalRollMicroseconds`, `maxRollMicroseconds`.

O relatório deve apresentar as diferenças, a razão `totalRollMicroseconds / rollAttempts` quando houver tentativas e o máximo observado. Não inferir percentis nem FPS a partir dessas medidas. Não enviar dados do usuário, inventário completo ou saves privados: registrar somente itens e resultados escolhidos para QA.

## Casos manuais obrigatórios

| ID | Procedimento | Aceitação |
| --- | --- | --- |
| P1 | Abrir mundo com ExampleModCSharp e reforjar o `ExampleWand` com pilha normal de 1 | Sem travamento; escolha condicional de `ExampleMightyPrefix` somente se categoria, restrições e valores atuais permitirem; `chosenItemRolls` acompanha a operação |
| P2 | Reforjar itens vanilla comuns sem condições C# | Reforja original funciona; os resultados continuam válidos |
| P3 | Aplicar prefixo C# com efeitos de dano/valor e reforjar novamente | O novo prefixo substitui o bônus anterior sem duplicação cumulativa |
| P4 | Equipar e remover acessório com prefixo C# | Defesa, movimento, tooltip e reversão do bônus correspondem ao item ativo |
| P5 | Sair do mundo, salvar, fechar aplicativo, reabrir e carregar o mesmo personagem e mundo | Prefixo e atributos reaparecem corretamente, sem bônus duplicado ou desaparecido |
| P6 | Carregar um prefixo salvo e alterar condições mutáveis de elegibilidade | A restauração não deve falhar apenas por mudança de condição de rolagem |
| P7 | Ativar um segundo mod com prefixos e repetir reforja vanilla e C# | Seleção mista sem substituir pesos nem bloquear prefixos alheios |
| P8 | Fazer a mesma série em multiplayer, quando houver host compatível | Prefixos, valores e efeitos não divergem entre clientes; caso contrário marcar *não testado* |
| P9 | Jogar e reforjar 30 vezes com e sem ExampleModCSharp, no mesmo cenário | Registrar FPS mínimo, médio, tempo de quadro e picos; procurar crashes e regressões, sem afirmar 60 FPS antes de medir |
| P10 | Consultar o diagnóstico no final de cada rodada | Contadores crescem coerentemente, `maxRollMicroseconds <= totalRollMicroseconds`, não há falha/crash no hook |

### Limite do teste por pilha

O `ExampleWand` é uma arma; não presumir que ela possa ter pilha 2–5 no inventário. Os intervalos de `stack` acima de 1 exigem um item de teste realmente empilhável que suporte prefixos, um fixture nativo com campos controlados ou uma implementação futura de cenário isolado. **Nunca** concluir que os intervalos 2–5 passaram apenas testando uma arma não empilhável. Fixtures C++ já exercitam esses intervalos, mas isso não demonstra gameplay Android.

## Registro do resultado

Guardar para cada caso: **PASS**, **FAIL** ou **NOT TESTED**, commit/build, mods ativos, passos de reprodução, resultado visível, contadores pré/pós, FPS e log de falha quando aplicável. `NOT TESTED` não conta como aprovação.

## Efeito dinâmico de acessórios C# (experimental)

O overload `BunnyPrefix.ApplyAccessoryEffects(BunnyPrefixPlayerSnapshot player, ref int defenseBonus)` recebe vida atual e máxima em snapshot imutável, opcionalmente no worker MonoVM. A política estática `ApplyAccessoryEffects(ref int)` permanece. A execução do worker tem limite por tentativa de 650 µs e cache nativo de 250 ms por jogador/prefixo/vida, com taxa máxima de uma tentativa a cada 150 ms por thread; em erros ou timeout, fica apenas o efeito compilado. O comportamento ainda exige testes reais de FPS, multiplayer, respawn e carregamento.

## Paridade ainda pendente

O PrefixLoader C# implementa decisões declarativas avaliadas em C++ e lógica C# avaliada na compilação. Os callbacks gerenciados arbitrários com objetos reais do Terraria, incluindo `ChoosePrefix(item, random)`, `CanRoll(item)`, `RollChance(item)`, `GetTooltipLines(item)`, `Apply(item)` e `ApplyAccessoryEffects(player)`, ainda **não** possuem paridade geral. Um callback síncrono na thread de reforja não deve ser adicionado sem protocolo com timeout, cancelamento de requisições tardias, isolamento da MonoVM, reprodução determinística, e testes reais de carga. Até lá a PR permanece draft.

## Preparação do AllowPrefix gerenciado por item

O SDK agora permite `AllowPrefix(BunnyPrefixItemSnapshot item,int prefix)` nas quatro famílias de itens e testa `BunnyItemPrefixRuntime.Dispatch` em fixture C#, sem compartilhar ponteiros IL2CPP. O caminho **ainda não é vinculado** aos hooks de Terraria: só a política declarativa `BunnyItemPrefixRules` participa das rolagens nativas. Agora o compilador emite `ITEM_PREFIX_CALLBACK` apenas para overrides reais, e o carregador valida identidade do item, máscara e duplicatas. Esse descriptor possui agora um binding nativo imutável por mod/tipo/classe, publicado atomicamente; o carregamento agora também vincula o assembly e agenda warmup separado no worker MonoVM. Os hooks ainda não consultam esse dispatcher. Antes de ativar, são necessários binding por mod e tipo, warmup isolado, cota de RPC por tentativa, cache por candidato, precedência do veto permanente, exclusão da restauração de saves e testes host/dispositivo. Não declarar paridade com `ModItem/GlobalItem.AllowPrefix(item,prefix)` neste estágio.
