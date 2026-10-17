# Compilador C# .85: experimento ARM64 AOT normal

## Evidência Android recebida da .84

ExampleMod 0.1.44, Xiaomi 2109119DG, Android 14: preparação com cache 576 ms e entrada no mundo 4,9 s. A primeira compilação medida separadamente levou 10.110 ms de parede e 9.839 ms de CPU do worker; parsing 656 ms, referências 24 ms, Emit 8.324 ms. A meta de primeira compilação em até 5 s **não foi atingida**. O Emit inclui trabalho semântico, geração de IL e possível JIT do compilador; esses números não isolam o custo de JIT.

## Implementação

A .85 preserva os commits de `codex/csharp-compact-compiler-v90-29-84` e integra a correção de relatórios de `codex/csharp-hook-registration-qa-v90-29-84`, incluindo ExampleMod 0.1.45. Nenhum exemplo é removido ou marcado como validado por esta alteração.

O build usa `Microsoft.NETCore.App.Runtime.AOT.linux-x64.Cross.android-arm64` 8.0.23, identificado no manifesto oficial do runtime .NET 8.0.23. Verifica SHA512 do pacote com o catálogo NuGet. Gera código ARM64 normal, sem LLVM, para CoreLib, Collections.Immutable, Reflection.Metadata, CodeAnalysis, CodeAnalysis.CSharp e BunnySourceCompiler. Não usa full AOT, hybrid, trimming, remoção de IL ou AOT dos mods. O Mono 8.0.23 com correções de sinais da .72 continua sendo o runtime do APK.

`tools/build_compiler_aot.py` produz `libbunnycompileraot.so` com tabela privada de módulos, hashes das assemblies e hash do runtime exato. `CompilerAot.h` valida todos os hashes, nomes, ABI e versão de formato 185 antes da primeira chamada irreversível a `mono_aot_register_module`. O código registrado permanece mapeado durante a vida do Mono. Falhas anteriores ao registro mantêm o caminho JIT e um motivo explícito. As validações de MVID e dependências do próprio Mono continuam habilitadas.

O registro ocorre no worker de compilação antes de inicializar o domínio; não adiciona código AOT aos mods nem altera as bridges de gameplay. O runtime não é substituído por um Mono oficial sem as correções de sinais.

O relatório inclui `source-compiler-aot-status.txt`, com PID, módulos registrados, motivo, setupMs e `compilerMethodCode`. A identificação `AOT` confirma que o endereço compilado de `Compiler.Compile` pertence ao bundle; registro sozinho não comprova uso de todos os métodos AOT, nem desempenho. `JITOrUnavailable` exige investigação de compatibilidade/aceitação do módulo e não é sucesso de aceleração.

## Validação

`tests/run_compiler_aot_loader.py` executa o carregador de produção com bibliotecas nativas de teste. Cobre sucesso e rejeição transacional de runtime, segunda dependência, formato, ABI, caminho e quantidade. Não simula execução Mono nem comprova tempo Android. O pipeline mantém regressões C#/nativas/JS, comparação executada dos defaults do ExampleMod com referências runtime/compactas, testes JVM, compilação Android e assinatura privada. `compiler-aot-build.json` registra tamanhos e tempos do build, nunca FPS ou tempo de compilação Android.

Primeiro CI: os seis módulos ARM64 foram gerados e ligados com sucesso. A checagem do SDK identificou documentação gerada desatualizada dos dois métodos de diagnóstico de hooks integrados; a referência foi corrigida, mantendo a checagem obrigatória. A entrega privada exige oito workflows independentes aprovados no mesmo commit. Estado: contratos locais aprovados; regressões finais e APK pendentes; execução e desempenho Android pendentes. Não é conclusão do Lote A.

## Teste no Terraria com ExampleMod

1. Instale APK experimental .85 e ExampleMod CSharp 0.1.45. Preserve saves e use somente este pacote do ExampleMod, sem probes antigos duplicados.
2. A alteração de SDK/exemplo deve provocar nova compilação; se o relatório ainda indicar cache, altere somente o texto retornado em `Common/Players/ExampleWelcomePlayer.cs` dentro do pacote editável e reabra. Não é necessário apagar dados do aplicativo.
3. Entre no mesmo mundo. Teste bancada C#, receitas de armadura, tooltips, escudo, botas, asas e prefixos existentes do ExampleMod.
4. Gere relatório após o carregamento. Envie `CompilerAot`, `RoslynProfile`, `MonoWorkerTimeline`, `DefaultsRequest`, `preparo` e entrada no mundo. Confira PID e idade para distinguir histórico de sessão atual.
5. Feche e reabra sem modificar fontes para medir cache. Teste novamente salvar/sair/entrar.

Meta <=5 s, ausência de regressões e FPS continuam aguardando medição real no Xiaomi. Se houver regressão, a .84 permanece disponível; não sobrescrevemos sua branch ou release.
