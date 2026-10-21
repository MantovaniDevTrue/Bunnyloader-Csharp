Consulte também [SHOOTING.md](SHOOTING.md): Example Escopeta + Example Bala, oito tiros, consumo, dano e persistência. Novas APIs aguardam teste no aparelho.

> Continuidade do port integral: áudio de ExampleGun em [CUSTOM_AUDIO.md](CUSTOM_AUDIO.md). Desenvolvimento sem nova release parcial; ainda requer validação no Android.

> .96: roteiro de armas, munição, offsets e idiomas em [NATIVE_DEFAULTS_V96.md](NATIVE_DEFAULTS_V96.md). Manter também o roteiro do minion da .95.2. Novos ports ainda aguardam validação Android.

# Continuidade .95 — teste o sistema de summons do ExampleMod

Leia [SUMMON_LOADER_V95.md](SUMMON_LOADER_V95.md). Cancelar o buff deve matar todos os minions daquele dono; reinvocar, seguir, combater e retornar são os testes prioritários. Esta entrega ainda aguarda teste Android.

> Atualização .93: veja PROJECTILE_LOADER_V93.md para testar minion, buff, cajado, políticas e regressão das receitas, todos no ExampleMod principal.

> Novos testes .92: [receitas de arma/acessório/poção na bancada C#](OWNED_RECIPE_LOADER_V92.md).

# Lote .90.28 — roteiro Terraria Android

Instale APK e ExampleMod 0.1.34 desta entrega, mantendo um mod JS ativo para
testar convivencia. Antes de alterar mundos/personagens de teste, mantenha seu
backup habitual. Plataforma: Terraria 1.4.5.8.6, Xiaomi 2109119DG, Android 14.

1. **Auto-carregamento:** abrir mundo; relatorio deve mostrar defaults aplicados
   para tres BunnyItems e `csharp recipe: ... instalada` para as tres receitas.
   Nao compilar DLL fora do aplicativo. Identificar qualquer estado failed/pending.
2. **Material:** com 10 Dirt Blocks e uma Work Bench, criar 999 Example Items.
   Conferir consumo exato, pilha 9999, valor interno 100 e armazenamento em bau.
3. **Picareta:** criar com 10 Example Items na Work Bench; conferir dano 20,
   repulsao 6, pick 220%, Green, uso/animacao 10 e som Item1. Testar mineracao
   de varios blocos/minerios (inclusive um que exija alto poder), uso continuo,
   auto-reuse e acertar NPCs como ferramenta melee.
4. **Hamaxe:** criar com 10 Example Items na mesma estacao. Conferir dano 25,
   axe 30 interno = 150% na interface, hammer 100%, uso/animacao 15 e som Item1.
   Cortar arvores e martelar blocos/plataformas/paredes conforme a acao nativa.
5. **Economia:** value=50000 para ambas ferramentas equivale a sellPrice de
   uma moeda de ouro no original. Conferir compra/venda e tooltip/raridade.
6. **Save/Exit:** guardar material/ferramentas no inventario e bau; sair/reabrir
   mundo, trocar loadout, morrer/respawn; conferir identidade, pilha e prefixo.
7. **Regressao:** armas/projeteis, buffs, prefixes, Guardian Shield, Trailblazer
   Boots e Mystic Ward anteriores; remover/equipar e trocar de mundo. Validar
   um item/receita JS junto dos C#, com UI/crafting/Guia sem duplicacoes.
8. **Desempenho:** registrar FPS em jogo ativo com movimento/mineracao/combate,
   comparar a .90.27.6 nas mesmas condicoes. Procurar crashes, espera por locks,
   repeticao de registros, memoria crescente e receitas que permaneçam pendentes.

Somente depois do roteiro, anexar relatorio com versao, exemplos exercitados,
resultado, FPS medido e eventuais falhas. Host/compilacao nao preenchem Validado.
ExampleItem.js ainda contem o helper de moeda customizada que esta pendente:
este lote implementa o item e sua receita, mas nao anuncia essa moeda/loja.
