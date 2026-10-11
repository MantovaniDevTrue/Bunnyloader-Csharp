# Contribuindo com a integração C#

Esta é uma **extensão experimental e independente** do [Bunny Loader de Jonata](https://github.com/JonataOliveiraa/Bunny-Loader). O objetivo não é reivindicar o projeto original nem prometer suporte completo ao tModLoader.

1. Trabalhe sobre a branch de desenvolvimento C#; mantenha snapshots de versão usados nos testes Android.
2. Use os fontes canônicos em `Split/Bunny-Loader/`; `src/` é apenas uma pasta de atalhos. Evite arquivos duplicados que saiam de sincronia.
3. Para mods, siga **ExampleModBunny** e os métodos efetivamente suportados pelo Bunny, não implementações copiadas de tModLoader.
4. Mantenha código organizado, sem comentários desnecessários e sem alterar efeitos de gameplay ao otimizar.
5. Para cada alteração de integração: teste automatizado apropriado, exemplo reproduzível, registro de limitações e teste real no Android quando necessário.
6. Diferencie explicitamente `implementado`, `aprovado no CI` e `validado no Android`.
7. Nunca inclua APK do Terraria, bibliotecas proprietárias do jogo, saves pessoais, logs com dados sensíveis ou arquivos internos de assinatura.

Para reportar regressões, forneça a versão/commit, `.bmod` utilizado, aparelho/Android, comandos executados, o que ocorreu e os logs. Consulte [TESTE-ANDROID-v60](docs/TESTE-ANDROID-v60.md).
