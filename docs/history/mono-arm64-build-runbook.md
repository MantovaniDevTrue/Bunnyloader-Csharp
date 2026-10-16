# Bunny C# v0.1.10.16.57 — compilação de teste

O workflow build-bunny-csharp-source-loader.yml é iniciado por push na branch bunny-csharp-source-0.1.10.16.57-mono-arm64-signals.

1. Faz checkout da tag .NET runtime v8.0.23 e altera só src/mono/mono/utils/mono-threads-posix-signals.c.
2. Recompila Mono para Android ARM64 com NDK e exige o símbolo exportado mono_bunny_android_signal_policy_v1.
3. Retira a libmonosgen-2.0.so oficial do pack e a substitui exclusivamente no APK experimental, mantendo as assemblies oficiais 8.0.23.
4. Empacota Terraria 1.4.5.8.6 e Exemplo C# com nome Bunny Loader C# Test e versionCode 82.
5. Exige validação e publica APK experimental prerelease. A versão .56 não é sobrescrita.

No aparelho: verificar MONO_ONLY/DOMAIN_ONLY/PINNED_MONO, o retorno 42, o registro de sinais alterados e estabilidade por no mínimo dez minutos com pausa/retomada. Para regressões, retornar ao APK estável e não ativar o Mono no processo do Terraria. A compilação e a estabilidade do APK dependem dos resultados reais do workflow e dos testes Android.
