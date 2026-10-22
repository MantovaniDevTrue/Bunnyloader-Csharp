# BiomeConversion C#

[main.cs](Playable/content/main.cs) contém a categoria concreta `ExampleStoneConversion`,
`PostSetupContent`, regras nativas e o disparo pelo `ConversionBolt` da arma
`ConversionWand`. As explicações estão dentro do código.

O exemplo altera terreno (Dirt → Stone e DirtUnsafe Wall → Stone Wall); use uma
cópia de mundo ao validar. A arma é obtida pela ferramenta de criação de itens
usada no seu ambiente de testes: a base `BunnyWeapon` deste checkpoint não expõe
receitas, e esta amostra não inventa uma receita que o host não executa.

Execute o teste completo, na raiz do repositório:

```sh
python3 Split/Bunny-Loader/tests/biome_conversion/run.py
```

Inclui compilação das fontes exatas pelo Roslyn, bootstrap e execução real da ponte
P/Invoke até o motor C++ com um mundo de teste. O adaptador de produção usa os
setters do Terraria Mobile e é verificado separadamente com metadata simulada.
As duas texturas reutilizam SourceWand/SourceBolt dos samples existentes.

[Contrato, adaptações e limites](../../../../docs/csharp-biome-conversion-loader.md).
Nenhum APK ou pacote foi produzido; validação no aparelho permanece pendente.
