# Bunny Loader — MonoVM ARM64 com sinais de tempo real

Base: MonoVM .NET 8.0.23, com os PRs #8 (worker GC-safe) e #9 (preflight de sinais) preservados.

## Implementação

Em Android ARM64, altera mono-threads-posix-signals.c para escolher três sinais de tempo real livres, usando a função mono_threads_suspend_search_alternative_signal já existente no Mono. Na inicialização de suspensão/reinício/interrupção, a própria instalação dos handlers torna o sinal recém-escolhido indisponível para a próxima seleção. Os alvos Android de 32 bits mantêm SIGPWR/SIGXCPU/SIGTTIN e a validação original de sinais menores que 32.

O símbolo exportado mono_bunny_android_signal_policy_v1 identifica a biblioteca recompilada. Sem esse símbolo, o Bunny continua com a proteção do PR #9, bloqueando inicialização do domínio quando os sinais fixos já estão ocupados. Com esse símbolo, verifica ao menos três sinais RT livres antes de mono_jit_init. Essa verificação não reserva os sinais e não impede uma corrida com outra biblioteca, portanto o teste em Android é obrigatório.

O build empacota as mesmas assemblies e bibliotecas auxiliares do pacote oficial Microsoft.NETCore.App.Runtime.Mono.android-arm64 8.0.23; substitui apenas libmonosgen-2.0.so pelo produto recompilado da tag v8.0.23. A build falha se o símbolo da política não estiver exportado.

## Validação pendente

A compilação no GitHub Actions não demonstra compatibilidade em execução real com Unity/IL2CPP. É necessário confirmar que a inicialização do domínio, o método gerenciado (42), coleta de lixo, suspensão/retomada do Terraria, carregamento e encerramento funcionam sem abortos. O erro de mutex destruído ainda pode ter causas não relacionadas aos sinais. A alteração não é uma correção garantida dos crashes.
