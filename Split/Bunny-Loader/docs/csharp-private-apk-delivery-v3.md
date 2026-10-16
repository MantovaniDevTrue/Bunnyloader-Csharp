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


## Migração das dependências de build — 12/10/2026

A release `bunny-private-build-inputs-v1` no repositório privado `MantovaniDevTrue/Bunnyloader-Csharp-Builds` contém três dependências de compilação essenciais preservadas por SHA-256:

- `BunnyRuntime_Terraria_1.4.5.8.6.zip`: `2eb8d02dea144cb2fff6a27b94cc371e50ffd9aa44ba4dcd123337691a54c0ab`
- `BunnyLoader-v0.1.9.1-MusicPrewarm.apk`: `aaf7c46046e776a966122d7f4e26469ebb72ad2fc21f0c6e57f98189e0c93c00`
- `BunnyLoader-CSharp-Test-v0.1.10.16.72-ManagedIngress.apk`: `84dd410f1054690cfb4bf6a74078c0bd2c92c7ee0ab431b345e0d5c84f353feb`

Os workflows desta branch os baixam **somente do repositório privado** com `BUNNY_PRIVATE_BUILD_TOKEN`, e continuam verificando os checksums. Não eliminar os arquivos públicos correspondentes até que os dois workflows sejam validados após esta alteração.

Após isso: apagar todas as APKs antigas e o runtime proprietary das releases públicas; preservar os `.bmod` e seus hashes enquanto aplicáveis. Para uma limpeza em massa, será necessário um token com escopo de escrita no **repositório público**, pois o token de destino privado não tem essa permissão. A chave v1 nunca deve ser restaurada. Históricos/forks e downloads existentes não podem ser revogados retroativamente.

## Limpeza pública concluída

- Execução GitHub Actions: https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/38218409767
- Dependências privadas migradas com hashes validados em `bunny-private-build-inputs-v1`.
- APK de teste assinada com v3 e criada apenas no privado: `csharp-v3-run-38217387836-a1`.
- Excluídos 262 anexos públicos: 133 APKs, 128 checksums de APK e o ZIP proprietário `BunnyRuntime_Terraria_1.4.5.8.6.zip`.
- Excluídas 9 releases que ficaram vazias, preservadas 128 releases e 402 assets (incluindo 190 `.bmod`).
- Conferência final da API GitHub pública: APK=0, APK.sha256=0, runtime ZIP=0, JKS=0.
- O workflow de exclusão foi desarmado (`BUNNY_EXECUTE_CLEANUP=false`) após a conclusão.
- Revogar o fine-grained personal access token temporário de limpeza e remover o secret `BUNNY_PUBLIC_CLEANUP_TOKEN`. Preservar `BUNNY_PRIVATE_BUILD_TOKEN` e os três segredos de assinatura v3.
- Exclusões de releases não desfazem downloads/forks/caches nem apagam a senha antiga do histórico de commits. Nunca recuperar/reutilizar a chave v1.
- Branch atual para futuros builds: `csharp-public-release-cleanup-v1`. Branches anteriores podem ainda conter workflows que publicam APKs; migrar mudanças futuras para esta baseline segura.
