# Descoberta automatica da API C# Bunny Loader

Este experimento produz:
- docs/generated/csharp_api_catalog.json: catalogo legivel por ferramentas
- docs/generated/csharp_api_catalog.md: diferencas JS / C# e classificacoes
- docs/generated/csharp_api_examples.md: exemplos C# tirados dos arquivos-fonte

O script le exclusivamente o ExampleModBunny JS, BunnyCsSdk e ExampleModCSharp existentes no repositorio. O arquivo coverage.json guarda evidencias com caminhos e trechos obrigatorios da implementacao. Descoberta estatica nao prova que o metodo JS funciona em C# e nao promove suporte experimental a device_verified.

Executar em Split/Bunny-Loader:

    python3 tools/csharp_api_discovery.py --write
    python3 tools/csharp_api_discovery.py --check
    python3 tests/test_csharp_api_discovery.py

A pipeline da branch experimental roda os testes de C# existentes e salva os tres relatórios na propria branch. A versao .86 e o ExampleModCSharp 0.1.14 permanecem inalterados. O proximo estagio pode automatizar a geracao de novos pequenos exemplos somente para assinaturas que estejam realmente implementadas no SDK.

Regras: nada de tModLoader; usar os fontes ExampleModBunny e Bunny; preservar diferenca entre device_verified, ci_verified, experimental e not_implemented; nao inferir compatibilidade multiplayer.
