# Aborto Mono antes do FORTIFY: evidência e contenção

Base: Bunny C# .56, commit 2668443ce2fc819f9d039bc7345e27a80b2e544d.

Captura do aparelho, 10/10/2026, PID 15060:

- 12:57:42.899: domain-thread-audit-running (thread proprietária 17377).
- 12:57:52.292/293: threads 15304 e 15401 registram `Thread ... may have been prematurely finalized` e a assertiva de `mono-threads.c:722`, `mono_thread_info_current`, condição `info`.
- 12:57:52.357: thread 15562 registra FORTIFY por mutex destruído.
- 12:57:52.500: Android confirma a morte do processo :game.

Portanto o FORTIFY não é o primeiro erro fatal registrado. Não há uma stack nativa completa nesta captura; não é possível atribuir definitivamente a assertiva a um sinal específico, ao GC ou à destruição de uma thread.

No código oficial dotnet/runtime v8.0.23 (`src/mono/mono/utils/mono-threads-posix-signals.c`), Android fixa SIGPWR para suspensão, SIGXCPU para retomada e SIGTTIN para interrupção. A instalação usa sigaction sem preservar o proprietário anterior. O handler de suspensão chama mono_thread_info_current, que exige que a thread esteja registrada no Mono. Receber nesses handlers um sinal destinado a uma thread de outro runtime é uma explicação compatível com a captura, ainda não comprovada no aparelho.

A proteção consulta os três handlers imediatamente antes de mono_jit_init, tanto no probe direto quanto nos modos por etapas. Se algum não estiver em SIG_DFL, retorna `mono-signal-conflict`, com o sinal e, quando dladdr identifica o endereço, a biblioteca proprietária. SIG_IGN também é preservado. Não instala handlers, não ignora assertivas, não modifica mutexes e não inicializa o domínio em caso de conflito. Os modos anteriores ao domínio continuam disponíveis.

Limites: a consulta não reserva sinais nem elimina uma corrida com outra biblioteca que altere handlers simultaneamente. Ela protege apenas proprietários visíveis naquele momento. Não reconstrói o Mono, não fornece multiplexação de sinais e não habilita C# no processo Unity quando há conflito. O worker isolado existente pode ser usado explicitamente; não há troca silenciosa de processo. Não altera APK, assinatura ou releases.

Validação: `python3 Split/Bunny-Loader/tests/run_managed_signal_ownership.py`. São 48 cenários nativos com Mono simulado e sigaction real: sinais livres, os três sinais ocupados, SIG_IGN, SA_SIGINFO e oito modos de inicialização. Conferem que conflitos impedem mono_jit_init e preservam o handler; não substituem execução Android.

O PR #8 (espera GC-safe) trata outro defeito e não deve ser apresentado como correção comprovada desta assertiva.
