# Mono owner: espera nativa compatível com GC

Base: branch bunny-csharp-source-0.1.10.16.56-mono-domain-thread-audit,
commit 2668443ce2fc819f9d039bc7345e27a80b2e544d.

Após mono_jit_init/mono_thread_attach, o worker persistente pode estar em
GC-unsafe. Entrar em std::condition_variable::wait nesse estado deixa a thread
registrada como executando código que o GC cooperativo precisa suspender, embora
ela esteja bloqueada em uma espera C++ sem pontos de cooperação.

A correção usa mono_threads_enter_gc_safe_region/exit_gc_safe_region em torno da
espera e da auditoria nativa. Mantém a thread proprietária anexada. A saída da região
ocorre depois de soltar o mutex da fila e antes de chamar Mono novamente. Cookies
nulos continuam tendo entrada/saída balanceadas. Antes de criar um domínio pelo
worker, a ausência das APIs necessárias produz pinned-gc-api-missing.

Referência: dotnet/runtime v8.0.23, src/mono/mono/metadata/threads.c documenta que
mono_thread_attach_external_native_thread retorna em GC Unsafe; os protótipos de
transição são públicos em src/mono/mono/utils/mono-threads-api.h. Os símbolos foram
também conferidos na biblioteca Android ARM64 8.0.23 anteriormente fornecida.

Validação: executar `python3 Split/Bunny-Loader/tests/run_managed_pinned_gc.py`.
O mock exige chamadas gerenciadas fora da região segura, comprova espera segura,
retomada repetida, timeout da auditoria, modos anteriores ao domínio e falha antes
da inicialização se faltar uma das APIs. Remover o escopo da espera faz o teste
DOMAIN_ONLY falhar. O smoke anterior de bridge/callbacks e o checkpoint de aborto
simulado também passaram localmente.

Limites: esses testes modelam o contrato nativo, não executam o GC real Android.
Não foi disponibilizado um log atual da versão .56 nesta análise; o log Fix11 da
conversa pertence a outro bootstrap. Portanto, esta mudança corrige um defeito
concreto encontrado na revisão, mas a relação com o encerramento observado no
aparelho ainda precisa ser confirmada. Não altera runtime, assinatura, versão,
releases ou política de execução dos mods. Outros caminhos que anexam threads
ao Mono precisam de auditoria própria de ciclo de vida.
