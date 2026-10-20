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

O PrefixLoader C# combina políticas nativas declarativas, configuração C# compilada e callbacks opcionais de snapshots imutáveis transportados ao worker MonoVM. Não oferece paridade geral com os hooks arbitrários de objetos reais do Terraria, como `ChoosePrefix(item, random)`, callbacks por instância que acessam objetos Item/Player completos, metadata rica de tooltip e mutações globais de itens. Os caminhos sincronizados têm orçamento limitado, cancelamento de respostas atrasadas e fallback para política nativa, mas ainda exigem medições e testes integrados no Android. Até lá, a PR permanece draft.

## AllowPrefix gerenciado por item — integração nativa experimental

O SDK oferece `AllowPrefix(BunnyPrefixItemSnapshot item,int prefix)` nas quatro famílias de itens. O compilador só emite `ITEM_PREFIX_CALLBACK` para overrides concretos; `SourceLoader` valida owner, tipo, máscara e duplicatas. O `ManagedPrefixBridge` publica o binding imutável `itemType -> {modId, classe}`, vincula o assembly e agenda warmup independente do worker MonoVM. O dispatcher C# recebe somente seis campos de Item em snapshot de leitura e o ID do prefixo, nunca ponteiros IL2CPP.

Os hooks nativos `GetRollablePrefixes`, `CanRollPrefix`, `RollAPrefix`, `Item.Prefix` e o cálculo de estatísticas consultam o callback apenas com warmup pronto. A prioridade é veto declarativo, depois callback opt-in: no máximo quatro solicitações por transação e orçamento total de 2500 µs, limitado a 650 µs por chamada. Cache por prefixo e assinatura atual do item evita RPC redundante. Erros, fila ocupada, timeout ou snapshot inválido retornam à decisão nativa, sem aplicar resposta tardia. No caminho protegido de restauração gerenciada (`g_restoreRoll`), filtros mutáveis e o worker são ignorados. Outros caminhos vanilla de carregamento de prefixos ainda exigem QA real. Sem veto efetivo e sem candidatos gerenciados, a rolagem e RNG vanilla originais permanecem intactos.

A fixture C# verifica o dispatcher e a emissão do descriptor. Os testes host C++ exercitam os hooks de produção com worker simulado: negativa/aceite, mudança de estado do item, reaproveitamento de cache, limite de chamadas, timeout/falha, chamada explícita, fallback, restauração de prefixos salvos e prefixos vanilla. O CI compila o código nativo ARM64, mas **não executa Terraria no Android**. A prova de gameplay, saves reais, clones, compatibilidade multijogador e FPS continua **NOT TESTED** até QA em APK privado autorizado. Não afirmar paridade total com o `ModItem/GlobalItem.AllowPrefix(item,prefix)` do ecossistema JavaScript.
