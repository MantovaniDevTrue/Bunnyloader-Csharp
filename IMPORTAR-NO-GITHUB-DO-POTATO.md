> **ATENÇÃO — guia histórico (26/10/2026).** Este procedimento foi escrito para uma entrega antiga, sobre a base do Bunny original `a370a664` de 15/10/2026. A `main` de Jonata já avançou e o script de importação exige um commit-base específico. **Não execute a etapa `--aplicar` nem substitua arquivos na `main` atual sem uma revisão de diferenças.** Para a entrega atual, leia primeiro [o guia de revisão para Jonata](docs/JONATA-REVISAO.md). Este texto anterior fica preservado para rastreabilidade.

---

# Importação da fonte C# integrada no Bunny Loader original

Preparei uma cópia **completa** do Bunny Loader original com o suporte C# já integrado: o código Android e C# está nos mesmos caminhos que o projeto original. Este guia mostra como importar os arquivos para **uma branch do repositório oficial**, preservando o histórico Git e sem mexer na `main` durante a revisão. Não é necessário portar cada classe manualmente.

## 1. Baixe os dois repositórios lado a lado

No terminal com Git e Python 3 instalados:

~~~bash
git clone --branch v1.0 --single-branch \
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

A cópia inclui todos os arquivos rastreados do projeto: C++, Kotlin, Java, projetos C# e exemplos, **fonte real do QuickJS-ng v0.17.0**, **runtime MonoVM/Roslyn já compilado e no lugar**, CMake, Gradle, documentação e os arquivos ocultos como `.github/`. Ela **não copia o `.git/`**, não executa `git clean`, não apaga arquivos do seu repositório, não faz commit e **não envia nada** para o GitHub automaticamente.

Também incluí [README-BUNNY-ORIGINAL.md](README-BUNNY-ORIGINAL.md), que guarda a documentação original, e [POTATO-C-SHARP-HANDOFF.md](POTATO-C-SHARP-HANDOFF.md), onde explico as mudanças.

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

- **ID do pacote:** já alterado para `com.bunnyloader`, igual ao original. Nenhuma edição de identificador é necessária para revisar a cópia. A assinatura oficial continua sob controle do mantenedor.
- **C# e MonoVM:** as DLLs do framework 8.0.23, Roslyn, compilador, SDK e bibliotecas ARM64 (incluindo Mono modificado) **já estão nos diretórios finais do projeto**. Não há scripts para executar depois de importar a cópia; o Gradle valida tudo automaticamente. Os [scripts e patch](tools/mono/README.md) ficaram apenas para manutenção futura.
- **Roslyn:** DLLs do compilador/SDK e do framework precisam ser geradas, empacotadas e conferidas no `assets/managed/framework.sha256`.
- **Terraria/Unity:** os binários e assets proprietários do jogo e a classe Unity permanecem fora deste Git, como já acontece no Bunny original.
- **CI:** o [workflow histórico de APK](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/blob/handoff/potato-bunny-csharp-20261018/.github/workflows/build-bunny-csharp-source-loader.yml) mostra o pipeline usado nos testes C#, mas inclui dependências de jogo e assinatura que não são nossa propriedade. **A correção do Mono está disponível independentemente** em código e script de compilação, sem secrets para gerar esse componente.


O inventário detalhado dos arquivos da integração está em [docs/POTATO-UPSTREAM-FILE-MAP.json](docs/POTATO-UPSTREAM-FILE-MAP.json). Minha explicação sobre startup, hook de entidades, Mono, Roslyn, JNI, CMake, SDK e testes fica no [manual completo](POTATO-C-SHARP-HANDOFF.md).

## 7. Como validar depois de importar

Eu começaria garantindo que o Bunny JS original ainda carrega seus exemplos, depois testaria o C# separadamente, e finalmente os dois motores juntos. A etapa Android deve incluir: iniciar/voltar de mundo, carregar automaticamente antes do mundo, receitas, save, acessórios, buffs, NPCs, projéteis, invocar/cancelar minions, morte/respawn, pausa/retomada, desempenho e ausência de crash de worker.

o loader C# tem seu próprio SDK e sua própria ponte, usando o Bunny como host.

Quando a build dele estiver validada com o identificador, assinatura e assets próprios, a integração pode ser promovida à linha oficial mantendo a autoria original do Bunny, meus créditos da extensão C# e os avisos de licenciamento.
