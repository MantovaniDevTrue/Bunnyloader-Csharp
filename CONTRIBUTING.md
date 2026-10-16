# Contribuindo

Esta é uma extensão independente do [Bunny Loader de Jonata](https://github.com/JonataOliveiraa/Bunny-Loader).
Consulte [licença e escopo](Split/Bunny-Loader/LICENSING.md) antes de redistribuir.

## Fluxo de trabalho

1. Parta da `main` integrada; declare dependências quando trabalhar sobre outra PR.
2. Edite os fontes canônicos em `Split/Bunny-Loader/`. Use o [mapa](docs/REPOSITORIO.md).
3. Para mods, siga ExampleModBunny e o SDK C# compilado. Referências de outros
   loaders exigem adaptação; não importe suas classes como API do Bunny.
4. Faça mudanças por responsabilidade. Preserve comportamento, assets, mira,
   efeitos e contratos de thread/GC ao otimizar. Meça o custo real.
5. Rode os [testes pertinentes](tests/README.md); descreva o que comprovam e seus limites.
6. Atualize [contratos](docs/SDK-CSharp.md), exemplos, referência gerada e
   [roadmap](docs/SDK-ROADMAP.md) quando mudar a API. Relatórios de versão ficam em `docs/history/`.
7. Se entregar novo APK, registre versão/commit/run e hashes em
   [checkpoint.json](docs/checkpoint.json), [status](docs/STATUS.md) e changelog.
   Confirmação no Android exige evidência real; CI e teste no aparelho são estados distintos.
8. Rode `python3 tools/check_repository.py` na raiz antes de abrir a PR.

## Pacotes e builds

Mantenha UID e nomes de conteúdo para atualizar o mesmo mod. Aumente a versão
quando seus bytes mudarem; use o pacote correspondente à release do APK.
Documentação e organização não precisam incrementar APK nem disparar build
Android. [Receita de build e workflows](docs/BUILD.md).

Não inclua binários/assets proprietários do Terraria, dumps completos, saves,
logs pessoais, tokens ou assinatura no Git público. Novos exemplos C# precisam
funcionar sem JS e não devem conter fallbacks para o motor de scripts.

## Bugs e revisão

Informe versão/commit, pacote/hash, aparelho/Android, passos, resultado esperado
e observado. Anexe o relatório da sessão pertinente, removendo dados pessoais.
[Roteiro de validação](docs/SDK-VALIDACAO.md).

A PR deve explicar problema, comportamento final, validação e pendências reais.
Não marcar exemplos inteiros como concluídos com base apenas na compilação.
