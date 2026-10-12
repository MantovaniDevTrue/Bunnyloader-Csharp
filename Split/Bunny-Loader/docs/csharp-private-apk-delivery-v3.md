# Bunny Loader C# — APK completa em repositório privado (v3)

Esta branch mantém a compilação no GitHub Actions do repositório público `MantovaniDevTrue/Bunnyloader-Csharp`, mas proíbe a distribuição pública de APKs completas. A assinatura continua usando `BUNNY_KEYSTORE_B64`, `BUNNY_SIGNING_PASSWORD` e `BUNNY_SIGNING_ALIAS`, guardados como repository secrets.

## Uma configuração, sem build privado

1. Criar no GitHub um repositório **Private** chamado `MantovaniDevTrue/Bunnyloader-Csharp-Builds` e marcar **Add a README** para que a branch `main` exista.
2. Criar um *fine-grained personal access token* em GitHub > Settings > Developer settings > Personal access tokens > Fine-grained tokens, com acesso **somente** a `Bunnyloader-Csharp-Builds` e **Repository permissions: Contents — Read and write**. O token deve ter uma expiração curta e ser renovado quando necessário. O token não deve ser enviado em chats ou arquivos.
3. No repositório **público** `MantovaniDevTrue/Bunnyloader-Csharp`, ir a Settings > Secrets and variables > Actions > New repository secret. Criar **`BUNNY_PRIVATE_BUILD_TOKEN`** com o valor do token.
4. Acionar o workflow `Build Bunny CSharp Source Files Prototype` no branch `csharp-private-apk-delivery-v3`, por commit ou `workflow_dispatch` quando disponível.

O workflow produz a APK `com.bunnyloader.csharptest` em um runner público efêmero, confere sua assinatura e exige que a API do GitHub reporte **`private=true`** para o repositório destinatário. Só então usa `gh release create` para criar uma **pré-release privada** com APK e SHA-256. O nome da tag é `csharp-v3-run-<run_id>-a<attempt>`. O arquivo temporário é eliminado após o upload, e a APK original é eliminada antes do artifact público da execução.

Se o token não estiver configurado, a publicação privada é **ignorada** com aviso, sem revelar os segredos. Se o destino não for privado, o upload é **recusado**. Não existe fallback para release, artifact ou outro endpoint público.

Os artifacts públicos continuam contendo apenas `ExampleModCSharp`, `SecondExampleModCSharp` e `SourceCSharpMod` (`.bmod` e hashes). **Não coloque APKs completas como Artifacts de Actions no repositório público.**

## Segurança e limitações

- **O repositório privado não compila**: serve somente para guardar releases, sem consumir minutos de Actions nele. São usados os minutos do repositório público.
- O token `BUNNY_PRIVATE_BUILD_TOKEN` é injetado somente na etapa final de envio, e nunca armazenado em fonte, logs ou arquivos. Restrinja colaboradores com acesso de escrita às branches que executam secrets e revise alterações em workflows.
- Este mecanismo **não apaga** releases antigas com a chave comprometida nem versões antigas com arquivos do Terraria. A limpeza dessas publicações requer revisão manual; a chave v1 deve ser tratada como comprometida.
- A entrega privada **não resolve por si só** os direitos sobre binários/recursos do Terraria; restrinja uso e distribuição a condições autorizadas. O processo atual mantém o runtime de desenvolvimento por ora, sem alterar MonoVM, C# ou gameplay.
- A nova assinatura v3 normalmente exige desinstalar a APK antiga assinada com v1 antes de instalar uma APK v3 de mesmo `applicationId`. Instalação limpa pode apagar saves, configs e packs locais.
- Se a publicação privada falhar, confira o nome exato do repositório, sua visibilidade, a existência da branch `main`, o token e as permissões Contents Read and write.

## Acompanhar os resultados

Workflow público: `https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/workflows/build-bunny-csharp-source-loader.yml`

Releases privadas: `https://github.com/MantovaniDevTrue/Bunnyloader-Csharp-Builds/releases` (somente pessoas com permissão).
