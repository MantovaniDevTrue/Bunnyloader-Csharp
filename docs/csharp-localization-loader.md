# LocalizationLoader C# — implementação inicial isolada

Estado: **incompleto; sem autorização de APK ou pacote jogável**.

O contrato foi extraído dos fontes Bunny `ModLocalization.js` e `Loaders/LocalizationLoader.js`. O caminho C# usa o SDK próprio, sem chamar QuickJS, ExMod ou tModLoader. Esta branch não modifica a PR de mundo (#98) nem a PR de equipamentos (#100).

O núcleo `BunnyLocalizationCatalog` oferece registros por mod e cultura, nomes completos `Mods.<id>.<chave>`, leitura relativa ao mod, desambiguação entre mods, fallback cultura → `en-US` → primeira cultura, substituição de `{$chave}` com escopos e índices formatados `@N`, registro de chaves globais, troca de idioma, remoção por mod e snapshots estáveis para a thread nativa. Entradas são verificadas antes de publicar um snapshot e os leitores não compartilham dicionários mutáveis. Limites de tamanho evitam catálogos malformados de tamanho ilimitado.

Teste host: `dotnet run --project Split/Bunny-Loader/tests/localization_loader/LocalizationLoader.csproj --configuration Release`. A Action `CSharp Localization Core` executa a suíte sem compilar Android ou criar APK.

## Para declarar o loader completo ainda será necessário

1. Carregar e achatar `Localization/<cultura>.json` e preservar as entradas originais do ExampleModBunny.
2. Ligar a detecção de mods e a sessão C# ao registro e descarte dos catálogos; preservar identidade e isolar erros por mod.
3. Publicar o snapshot no `Terraria.Localization.LanguageManager` pela thread de jogo, sem QuickJS; manter instâncias `LocalizedText` existentes e recarregar na troca de idioma.
4. Tratar referências a chaves vanilla e catálogos tardios, variantes `$`, carga/descarga, mudança de mundo e precedência de `Register` de forma fiel à referência.
5. Verificar ABI, integração Android, troca real de idiomas, localizações em itens/NPCs/buffs/config e FPS sob carga. A suíte de host não certifica nenhuma dessas etapas.

**Gate:** não marcar `LocalizationLoader` como completo, não alterar a matriz para validado, não gerar APK nem `.bmod` até concluir o contrato e a validação exigida.
