# LocalizationLoader, RarityLoader e AnglerQuestLoader

Este lote fecha o ciclo de implementação e testes de host dos três contratos
C# correspondentes ao Bunny JS 1.9.0. Inclui carga, callbacks, falhas, troca de
idioma, recarga e unload seletivo. A execução no Android permanece sem validar;
não há APK gerado e o bloqueio global de empacotamento permanece ativo.

| Loader | Contrato implementado e exercitado | Validação |
| --- | --- | --- |
| LocalizationLoader | JSON por cultura, UID real, fallback, referências, textos do jogo, registro dinâmico, troca de idioma, revisão da DLL, respostas obsoletas e retirada seletiva | 29 testes gerenciados existentes; fixtures de wire/session/retirement; publisher real no novo fixture de epoch; teste Roslyn integrado |
| RarityLoader | IDs compartilhados com JS, cores estáticas/dinâmicas, prefixos, nomes de itens, texto no chão, popups, persistência por chave estável, recarga e retirada seletiva | Suíte de raridade existente; layout MouseTextCache; registro imutável; dicionário real do renderer com falhas simuladas; teste Roslyn integrado |
| AnglerQuestLoader | Registro/remoção de peixes, flags e lista nativas, seleção diária, disponibilidade, descrição/localização, captura e callbacks por jogador, revisões de DLL e retirada de delegates | Fixtures IL2CPP de seleção/chat/captura; propriedades estáticas Android e campos legados; contextos coletáveis; scheduler real de teardown; teste Roslyn integrado |

## Revisão compartilhada da DLL

O host gera `|bunny-revision=N` ao vincular uma DLL. Os três loaders usam o mesmo
contexto gerenciado dessa revisão por UID, preservando campos estáticos do mod e
a identidade da assembly que registra textos durante a execução. A revisão não
é um caminho de arquivo: o carregador valida e remove o sufixo antes de ler a
DLL. Uma revisão nova lê os bytes novos mesmo quando o caminho físico é igual.

O teste `tests/three_loaders/Program.cs` compila um mod real com Roslyn, executa
raridade → localização → quest e exige que a quest observe as duas alterações
anteriores no estado estático. Recompila a mesma DLL com outro valor, preserva
um segundo UID na versão anterior e verifica que os contextos retirados são
coletáveis depois de liberar os três caches. As chamadas legadas de localização
sem token mantêm seu comportamento anterior; o caminho de produção usa tokens.

## DefaultToQuestFish conferido no Android

O helper do Android 301720 em `0xF54A7C` escreve `questItem=true` no offset
`0x20`, largura/altura 26 em `0x10/0x14` e `rare=-11` em `0xE8`.
A inspeção ARM64 usa o binário de SHA-256
`ff1f16f3ef122cde66f549096bca0225c46fd9a59e736c05706a8b0444d2ae06`.
O SDK agora reproduz esses valores; o protocolo aceita a raridade especial
Quest sem aceitar os números reservados -10..-2. `questItem` é publicado
como Boolean tipado por NativeDefaults. O teste do probe verifica o wire
emitido pelo peixe real e o fixture nativo protege o byte vizinho ao campo.
Isso é inspeção do runtime de referência, sem execução no aparelho.

## Retirada e recuperação

- Localização retira textos e invalida a geração do worker por UID. Uma resposta
  antiga não pode repor traduções após unload ou recarga no mesmo caminho.
  Repetir unload sem binding não avança indevidamente o piso de gerações.
- Raridade mantém IDs reservados para saves, mas desativa lookup, amostragem e
  cores do dono retirado. Um registro novo pode reativar a mesma identidade.
  O dicionário do Terraria é limpo na thread do jogo; valores substituídos por
  terceiros são preservados. Falhas de leitura/remoção são tentadas novamente.
- Quests deixam de executar imediatamente quando a geração do registro muda.
  A lista e as flags são reconciliadas na thread do jogo. A retirada dos
  delegates gerenciados não depende do prazo de 1,5 ms usado por gameplay.
- O scheduler retém comandos de teardown até `CLEARED`, `NOT_BOUND` ou binding
  antigo comprovadamente substituído. Uma falha temporária adia a nova chamada
  desse dono e permite que outros donos continuem. Reset, recarga e falhas
  tardias de defaults acionam os três caminhos de retirada.

Execute `bash tests/three_loaders/run.sh` para os testes novos e
`python3 tools/ci/test_ready_prs.py` para a integração completa. São testes sem
QuickJS, APK ou aparelho. O workflow dedicado só compila e executa testes host.

## Referência e limites da conclusão

O comportamento de referência é o Bunny JS anexado. O código tModLoader foi
consultado no commit `c9545cf27d56c456321e1896274c02adf67d0140`:
[RarityLoader.cs](https://github.com/tModLoader/tModLoader/blob/c9545cf27d56c456321e1896274c02adf67d0140/patches/tModLoader/Terraria/ModLoader/RarityLoader.cs),
[LocalizationLoader.cs](https://github.com/tModLoader/tModLoader/blob/c9545cf27d56c456321e1896274c02adf67d0140/patches/tModLoader/Terraria/ModLoader/LocalizationLoader.cs)
e os hooks de quests de
[ItemLoader.cs](https://github.com/tModLoader/tModLoader/blob/c9545cf27d56c456321e1896274c02adf67d0140/patches/tModLoader/Terraria/ModLoader/ItemLoader.cs).
No tModLoader, as quests pertencem ao ItemLoader; AnglerQuestLoader é a divisão
do Bunny. O Bunny usa JSON; ferramentas de edição/migração HJSON do tModLoader
não estão incluídas neste contrato. Isso não declara paridade integral com toda
a superfície do tModLoader, nem conclusão dos outros loaders do repositório.

Ainda precisa de confirmação no aparelho: instalação dos hooks, aparência das
cores/traduções, gameplay das quests e comportamento do MonoVM do Android.
O teste de contexto coletável executa no .NET host; não mede a coleta no MonoVM
Android. A conclusão do lote é de código e contratos host, sem selo de QA Android.
