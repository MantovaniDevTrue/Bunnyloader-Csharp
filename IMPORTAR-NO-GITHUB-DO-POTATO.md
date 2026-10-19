# Como eu preparei a fonte para você colocar no seu GitHub

Potato, já deixei uma cópia **completa** do seu Bunny original com o meu suporte a C# integrado. Você não precisa abrir cada arquivo para fazer o port: os arquivos estão nas mesmas pastas do projeto Android original. Escrevi este guia para você trazer a fonte para **uma branch do seu próprio repositório**, sem perder seu histórico de commits nem sobrescrever a sua `main` antes de avaliar o resultado.

## 1. Baixe os dois repositórios lado a lado

No terminal com Git e Python 3 instalados:

~~~bash
git clone --branch entrega/potato-bunny-original-com-csharp-20261018 --single-branch \
  https://github.com/MantovaniDevTrue/Bunnyloader-Csharp.git bunny-csharp-entrega

git clone https://github.com/JonataOliveiraa/Bunny-Loader.git bunny-original
~~~

Para este snapshot, comparei e preparei a integração sobre o seu commit `a370a664aa3b808c87e4e4f570b27504d20df723` (15/10/2026). Por segurança, o script de importação **recusa** uma base diferente. Se você já adicionou commits novos ao Bunny original, não aplique silenciosamente o snapshot sobre eles: primeiro compare e mescle essas mudanças.

## 2. Crie uma branch no seu projeto, mantendo o histórico

~~~bash
cd bunny-original
git switch -c feature/bunny-csharp-integrado
git status --short
cd ..
~~~

É importante que o clone esteja limpo. A cópia de fonte será um **commit novo na história do seu repositório original**, não um force push da minha branch.

## 3. Simule a cópia antes de escrever arquivos

~~~bash
python3 bunny-csharp-entrega/tools/importar_fonte_csharp_no_bunny.py \
  --destino bunny-original
~~~

O script confere o commit de base, identifica o projeto Android, verifica se não existem alterações não commitadas e informa quantos arquivos vai colocar. Sem `--aplicar`, ele **não escreve nada**.

## 4. Copie os arquivos fonte já integrados

~~~bash
python3 bunny-csharp-entrega/tools/importar_fonte_csharp_no_bunny.py \
  --destino bunny-original --aplicar

cd bunny-original
git status --short
git diff --stat
~~~

A cópia inclui os arquivos rastreados do projeto: C++, Kotlin, Java, projetos C# e exemplos, QuickJS, CMake, Gradle, documentação e os arquivos ocultos como `.github/`. Ela **não copia o `.git/`**, não executa `git clean`, não apaga arquivos do seu repositório, não faz commit e **não envia nada** para o GitHub automaticamente.

Também incluí [README-BUNNY-ORIGINAL.md](README-BUNNY-ORIGINAL.md), que guarda a documentação original, e [POTATO-C-SHARP-HANDOFF.md](POTATO-C-SHARP-HANDOFF.md), onde explico as mudanças. O README principal está em primeira pessoa, como minha apresentação da integração.

## 5. Quando estiver satisfeito com o diff, envie a sua branch

~~~bash
git add -A
git diff --cached --stat
git commit -m "feat: integrar suporte CSharp MonoVM e Roslyn ao Bunny Loader"
git push -u origin feature/bunny-csharp-integrado
~~~

Assim você terá todos os meus fontes adicionados ao seu GitHub para revisar, abrir PR e, quando achar adequado, mesclar na `main` oficial. Eu **não** alterei a sua `main` nem publiquei código no seu repositório.

Se preferir baixar um ZIP pelo GitHub, selecione a branch `entrega/potato-bunny-original-com-csharp-20261018` e use **Code → Download ZIP**. Extraia o conteúdo como fonte de referência. Para preservar corretamente o histórico Git oficial, recomendo o processo acima, com os dois clones.

## 6. O que precisa ser ajustado antes de um APK OFICIAL

Eu preservei o código da build experimental para manter a fidelidade com a versão que passou nos testes automatizados. Isso implica alguns cuidados:

- **ID do pacote:** a versão C# usa `com.bunnyloader.csharptest` em `app/build.gradle.kts`. Não alterei diretamente para o ID oficial sem uma nova rodada de build e teste. Você decide quando substituir pelo `com.bunnyloader` do seu projeto oficial, incluindo os caminhos internos e a assinatura.
- **MonoVM:** os fontes usam uma `libmonosgen-2.0.so` modificada para o Android, com a política `mono_bunny_android_signal_policy_v1`, recuperada por um workflow privado. Essa biblioteca não é incluída como binário no Git público; não substitua por uma Mono genérica sem testar.
- **Roslyn:** DLLs do compilador/SDK e do framework precisam ser geradas, empacotadas e conferidas no `assets/managed/framework.sha256`.
- **Terraria/Unity:** os binários e assets proprietários do jogo e a classe Unity permanecem fora deste Git, como já acontece no Bunny original.
- **CI:** o meu pipeline privado de referência está no [repositório de desenvolvimento](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/handoff/potato-bunny-csharp-20261018/.github/workflows/build-bunny-csharp-source-loader.yml), não faz parte da build GitHub oficial e precisa ser adaptado às suas credenciais e dependências, se você quiser automatizar a compilação.
- **Validação no Android:** o commit de produção C# `018b477a5a90115f399aed57ff84811c020d7a3f` passou no CI, mas o cenário de summon/minion do `.95.2` ainda está pendente no dispositivo. Na `.95.1`, o problema real era o campo `Projectile.identity` ausente; a nova fonte corrige o contrato sem afirmar que o aparelho já foi retestado.

O inventário detalhado dos arquivos da integração está em [docs/POTATO-UPSTREAM-FILE-MAP.json](docs/POTATO-UPSTREAM-FILE-MAP.json). Minha explicação sobre startup, hook de entidades, Mono, Roslyn, JNI, CMake, SDK e testes fica no [manual completo](POTATO-C-SHARP-HANDOFF.md).

## 7. Como validar depois de importar

Eu começaria garantindo que o Bunny JS original ainda carrega seus exemplos, depois testaria o C# separadamente, e finalmente os dois motores juntos. A etapa Android deve incluir: iniciar/voltar de mundo, carregar automaticamente antes do mundo, receitas, save, acessórios, buffs, NPCs, projéteis, invocar/cancelar minions, morte/respawn, pausa/retomada, desempenho e ausência de crash de worker.

**Não é necessário executar tModLoader ou usar o ExMod/Thorium para criar mods C# aqui:** o loader C# tem seu próprio SDK e sua própria ponte, usando o Bunny como host.

Quando a build dele estiver validada com o identificador, assinatura e assets próprios, a integração pode ser promovida à linha oficial mantendo a autoria original do Bunny, meus créditos da extensão C# e os avisos de licenciamento.

*Mantovani*
