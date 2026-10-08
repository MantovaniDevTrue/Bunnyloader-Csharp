# Bunny Loader — execução de mods C# por fonte

Base: `bunny-csharp-source-0.1.10.11-interpreter`.
Desenvolvimento isolado: `bunny-csharp-source-0.1.10.12-managed-runtime`.

## Objetivo

Instalar um `.bmod` com arquivos `.cs` e recursos sem exigir `native/arm64-v8a/libmod.so`, `.dll` fornecida pelo autor, ferramenta externa ou compilação manual. O Bunny deve preparar e executar o código no próprio Android.

O interpretador `SourceParser.cpp` de 0.1.10.11 somente reconhece classes `BunnyWeapon` e `BunnyProjectile` e atribuições literais em `SetDefaults()`. Ele não é execução C# geral nem substitui um runtime.

## Arquitetura escolhida para investigação

1. Preservar QuickJS e o carregador NativeAOT existentes.
2. Embutir um runtime Mono compatível com arm64 e Android como dependência do próprio APK, sem bibliotecas nativas por mod.
3. Inicializar o runtime em uma etapa controlada, fora da inicialização IL2CPP e antes de carregar assemblies dos mods.
4. Embutir o compilador C# Roslyn e assemblies de referência necessários, instalados junto com o APK. A compilação acontece internamente após importar ou atualizar o pacote, sem serviço externo.
5. Salvar assemblies compilados no armazenamento privado, com cache por hash dos fontes, versão do SDK e ABI.
6. Carregar os assemblies C# com isolamento lógico por mod e validar o contrato do SDK do Bunny antes de registrar conteúdo.
7. Encaminhar chamadas entre runtime gerenciado e Terraria IL2CPP somente pela Host API nativa já validada: conteúdo, projéteis, NPCs, buffs e acessórios.
8. Aplicar a sincronização da direção do jogador com a mira antes do disparo nos hooks de uso de armas.
9. Preservar efeitos visuais e reduzir trabalho por quadro: compilar uma vez, fazer despacho por tipo e cachear referências.
10. Registrar falhas de compilação, carga e execução com arquivo e linha, sem derrubar os mods JS.

## Formato esperado

```
manifest.json
content/main.cs
content/Content/Items/Weapons/ExampleWand.cs
content/Content/Projectiles/ExampleBolt.cs
content/Content/Items/Weapons/ExampleWand.png
content/Content/Projectiles/ExampleBolt.png
```

O formato de classes C# deve seguir o SDK Bunny, usando os exemplos de `samples/` e `experiments/csharp/BunnyCsSdk/` como fontes. Não copiar código de mods tModLoader como implementação do Bunny.

## Entregas verificáveis

- **Runtime:** carregar o Mono e executar um método C# real dentro do processo; uma análise de texto ou um reconhecimento de `.cs` não é suficiente.
- **Compilador:** importar um mod contendo somente `.cs` e PNG e compilar offline, no Android, sem `libmod.so` no pacote.
- **Host API:** executar `SetDefaults()` e pelo menos um callback C# periódico real (`AI()` de projétil ou NPC).
- **Teste de jogo:** registrar e disparar uma arma, validar posição/direção da mira, colisão, dano, descarregamento e reinicialização.
- **Compatibilidade:** repetir teste com mod JavaScript e mod NativeAOT já existente habilitados.
- **Desempenho:** medir tempo de compilação, memória do runtime e custo de callbacks, evitando trabalho de compilação durante o gameplay.

## Limites

- Mod C# escrito para o SDK do Bunny não é automaticamente um mod de PC/tModLoader.
- Não prometer execução de `AI()` antes de existir chamada real em código gerenciado.
- Assemblies C# carregados no processo não oferecem isolamento de segurança forte; instalar somente mods confiáveis.
- A integração Mono/Roslyn depende de validar as bibliotecas e dependências Android/arm64 antes de gerar um APK público.
