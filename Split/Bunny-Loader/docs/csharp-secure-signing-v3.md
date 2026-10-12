# Bunny Loader C# — Assinatura v3, GitHub público e distribuição

A chave de assinatura antiga está comprometida: a keystore foi anexada à release pública `bunny-signing-key-v1` e sua senha apareceu no workflow versionado. Apagar arquivos atuais não elimina o histórico de commits, downloads e caches. Não voltar a usá-la.

## Segredos da nova chave

No GitHub > Settings > Secrets and variables > Actions (segredos do repositório):

- `BUNNY_KEYSTORE_B64`: representação Base64 da chave `bunny-v3.jks`, nunca publicada.
- `BUNNY_SIGNING_PASSWORD`: senha do keystore (no Gradle atual também é a senha da chave).
- `BUNNY_SIGNING_ALIAS`: `bunnyv3`.

A verificação rápida `.github/workflows/bunny-csharp-signing-v3-smoke.yml` valida material, senha e alias com `keytool` sem imprimir dados sensíveis. O workflow de build principal reconstitui a chave apenas no `RUNNER_TEMP`, durante a própria etapa Gradle, e remove o arquivo ao término. Não são gravados segredos em `GITHUB_ENV`, no repositório, em releases nem em artifacts.

Não habilite execução de trabalhos com segredos para pull requests de terceiros e não aceite código não revisado em branches confiáveis. Idealmente, use um GitHub Environment protegido para assinatura e distribuição no futuro.

## Distribuição pública

Os artifacts do GitHub Actions nesta branch contêm somente os pacotes C# de exemplo em `.bmod` e seus SHA-256 (retenção de 14 dias), nunca APKs completos, `libil2cpp.so`, `libunity.so`, metadata e assets do Terraria, nem a chave. A API de releases retornou 403 nesta configuração pública, por isso usamos `actions/upload-artifact@v4` com `contents: read`.

O pipeline ainda usa o pacote `runtime-1.4.5.8.6` e outros APKs publicados anteriormente para gerar e testar o instalador em um runner efêmero. Isso não significa que o conteúdo desses arquivos esteja licenciado para distribuição. **Para uma distribuição pública regular, os binários e recursos proprietários precisam ser obtidos legitimamente pelo próprio usuário no dispositivo de destino ou por integração local autorizada.** A montagem local ainda é um trabalho pendente; esta mudança protege a assinatura, suspende novos APKs públicos e disponibiliza somente exemplos nos artifacts, mas não implementa a montagem local.

## Limpeza obrigatória das releases antigas

Pelo GitHub web, remova imediatamente o anexo de `bunny-signing-key-v1` e qualquer runtime, sprite, imagem ou APK de jogo cujo conteúdo não tenha direito de redistribuição, incluindo versões de testes C#. Revise todas as releases e tags do projeto. Uma exclusão posterior não recupera dados já baixados ou armazenados em forks/caches.

## Migração do Android

A nova chave v3 não é a mesma que assinou `com.bunnyloader.csharptest` anteriormente. Uma instalação com a assinatura antiga não poderá normalmente ser atualizada por cima de uma APK v3 com o mesmo package name. Antes de testar: backup do mundo, personagem e diretórios do loader; migração de assinatura ou instalação limpa, verificando o acesso aos saves. **Não desinstale sem backup.**

## Estado

- C# experimental .90.2 preservado em `csharp-prefix-reforge-v90-2-hotfix`.
- Nenhum comportamento de prefixo foi removido por estas alterações.
- Segredos não são verificáveis por leitura da API; o smoke de GitHub Actions fornece confirmação de execução sem revelar seus valores.
- Todo APK completo do Terraria é temporário, restrito ao job; releases futuras exigem um fluxo local autorizado.
