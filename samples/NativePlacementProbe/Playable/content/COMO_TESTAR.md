# Teste de colocacao C# — 0.1.0

Requer Bunny Loader .90.29.5 ou posterior. Nao depende do ExampleMod nem de JS.
Este pacote testa itens C# colocando tiles VANILLA, nao cria a bancada C# propria.
Os PNGs de inventario reutilizam assets do ExampleModBunny original.

1. Importe NativePlacementProbe-v0.1.0.bmod no gerenciador de mods e habilite-o.
2. Entre em um mundo de teste. Confira o relatorio: tres itens e fontes C# carregados,
   sem erro de compilacao, asset, registro ou colocacao.
3. Pegue terra e procure no crafting ou no menu de itens por "Teste C#". Os itens
   sao "Teste C# Bancada", "Teste C# Plataforma 0" e "Teste C# Plataforma 1".
   Uma terra fabrica dez unidades; nenhuma estacao e necessaria.
4. Coloque a bancada em chao plano com espaco de duas celulas. Uma unidade deve
   sair da pilha. Perto dela, confira uma receita vanilla que exija bancada,
   como porta de madeira, tendo seus ingredientes.
5. Coloque plataformas 0 e 1 lado a lado. Os estilos devem ter aparencia distinta;
   conte uma unidade consumida por colocacao. Compare as animacoes normais de uso.
6. Quebre os objetos. O Terraria deve devolver itens VANILLA, conforme tile/estilo;
   nao e esperado receber de volta o item "Teste C#". Isso e comportamento vanilla,
   e nao demonstra drops de tiles novos de mod.
7. Coloque novamente, guarde parte da pilha no inventario e parte em um bau.
   Salve/saia e reabra. Confira tiles, estilos, pilhas e capacidade de colocar os
   itens restaurados. Nao declarar que este teste valida persistencia de tiles C# novos.
8. Repita com ExampleMod C# habilitado junto, verificando que suas receitas e itens
   continuam presentes. Depois teste junto de um mod JS que voce ja utiliza.

Se algo falhar, envie o relatorio completo e diga qual item e etapa, se a pilha
foi consumida, qual tile apareceu e se a falha ocorreu antes ou depois de reabrir.
O resultado Android e FPS precisam ser medidos no aparelho; CI nao os comprova.
