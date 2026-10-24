# Bunny Loader C# — falha nativa na QA unificada de 24/10/2026

Base do relatório: APK QA 1.0.21, commit `5939f65535cdbd6d23635bcea8ce5ab505bac323`, Xiaomi 2109119DG, Android 14, Terraria 1.4.5.8.6, somente Five Loaders C# 0.1.1.

## Evidências registradas

- `SIGSEGV`, `SEGV_MAPERR`, endereço `0x0`, processo `17243`, thread `17464` (`Thread-12`).
- MonoVM: `phase=verified`, `result=42`; fontes do Five Loaders compilaram (`COMPILE_OK`).
- Os registros do mod chegam a `[CSharp][Touch] NativeHostReady Controls=1`, mas não há mensagem final `C# carregou sem problemas`.
- `universal-hooks-status.txt` ainda registra `state=queued`; não confirma qual callback causou o erro.
- `native-game-abort-trace.txt` e `native-game-module-maps.txt` foram preservados de sessões anteriores, e não trazem o PC da falha atual.
- Os 46 itens exibidos como `?` decorrem de carregar personagem contendo conteúdo do ExampleMod quando somente o Five Loaders está ativo; não demonstram corrupção.

## Problema identificado na instrumentação

No código-base `AutoSourceRuntime.cpp`, `installNativeSegvTrace()` era chamado somente após concluir o registro de todos os WorldSystems C#. O processo caiu antes desse ponto, portanto a captura não foi armada nesta sessão.

## Ajuste de instrumentação

- Instalar captura nativa SIGSEGV depois do MonoVM verificar `result=42`, mas antes da chamada de registro dos módulos C#.
- Atualizar a lista de bibliotecas nativas após MonoVM estar carregado.
- Marcar em memória o TID do worker MonoVM e o estágio de invocação. Estágios: `210/211` registro do WorldSystem, `220/221` outros métodos WorldSystem, `310/311` registro de hooks universais, `410/411` registro de comandos.
- Gravar `state=registering` antes de invocar o registro de hooks universais. No crash, o handler preserva `segv_pc`, `segv_lr`, `segv_mono_worker_tid` e `segv_mono_worker_stage`, além do mapa de módulos.

**Este lote é instrumentação, não uma correção comprovada do SIGSEGV.** Não desabilita hooks nem altera o comportamento do ExampleMod. Executar testes Android antes de declarar sucesso.

## Critério do próximo teste Android

Usar um mundo separado, sem conteúdo ausente, e apenas `Five Loaders CSharp QA 0.1.1`. Se houver novo crash, enviar o novo relatório completo e preferencialmente o tombstone com `backtrace` / `pc`. Confirmar que o `pid` dos registros nativos é o mesmo da sessão atual antes de atribuir uma causa. Caso o jogo abra, executar `/lote` e verificar `mundo=True`, `updates=19/19`.
