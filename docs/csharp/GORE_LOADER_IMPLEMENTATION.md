# GoreLoader C# nativo — execução por etapas, sem APK

Base: `v1.0` (código-fonte Bunny Loader). Branch de desenvolvimento isolada.
Esta implementação **não substitui o GoreLoader JavaScript** e não usa APIs do tModLoader. 
Os PNGs presentes em `samples/ExampleModCSharp/Playable/content/Content/Gores/`
foram reaproveitados integralmente do ExampleModBunny do próprio repositório.

## Contrato de fidelidade

O [GoreLoader.js](../../app/src/main/cpp/script/js/mod/Loaders/GoreLoader.js)
reconhece todo `*.png` em uma pasta `Gores/` do conteúdo do mod, incluindo
`Content/NPCs/Gores/` e `Assets/Textures/Gores/`. O tipo é indexado pelo
nome do PNG sem extensão e **pertence ao mod**. O jogo usa
`TextureAssets.Gore[type]`, `GoreID.Sets.SpecialAI`,
`DisappearSpeed`, `DisappearSpeedAlpha`, `IsDrip` e
`ChildSafety.SafeGore`. O efeito real é criado pelo método nativo
`Terraria.Gore.NewGore(Vector2, Vector2, int, float)`, na thread do jogo.

A versão C# precisa entregar exatamente essa funcionalidade, sem
renderização falsa, sem despachar a execução por QuickJS, sem varredura por
quadro e sem exigir que o modder informe um tipo numérico global.

## Etapa 1 — implementada nesta branch

- `ManagedGoreDiscovery.h/.cpp`: visita os arquivos **uma vez no
  carregamento**, em ordem determinística. Rejeita links simbólicos,
  caminhos externos, duplicatas e cabeçalhos PNG inválidos, com limites
  de diretórios, quantidade de gores, dimensão e bytes.
- `SourceLoader.cpp`: executa a descoberta antes do registro dos
  conteúdos C#. O campo `SourceModStatus.goreSourcesDiscovered` informa
  quantos arquivos foram encontrados, **não** quantos tipos foram instalados.
- `ManagedGoreCatalog.h`: prepara a reserva atomica dos tipos por
  par `(mod UID, nome)`, aceita gores homonimos de mods diferentes e
  proibe realocar IDs durante a sessao. Ainda nao publica os IDs no jogo.
- `tests/csharp_gore_catalog_test.cpp`: testa a separacao entre mods,
  falhas de capacidade, indices estaveis e limpeza do catalogo.
- `tests/csharp_gore_discovery_test.cpp`: testa subpastas, nomes,
  ordenação, dimensões, PNG inválido, duplicatas, link simbólico,
  limite por mod e os PNGs originais do ExampleMod C#.
- `.github/workflows/csharp-gore-source-check.yml`: compilação C++20
  e teste de host, expressamente **sem build de APK**.

O scanner checa a assinatura/IHDR, mas ainda **não decodifica integralmente**
o PNG. A decodificação real e a confirmação de dimensões ocorrerão na
etapa de instalação por `content::loadTextureAsset`.

## Etapas obrigatórias antes de poder marcar como completo

1. **Registro nativo:** criar a tabela de identidades
   `(mod UID, nome da gore) → type`, reservando tipos exclusivos por
   mod, com erros claros e sem colisões com os tipos existentes.
2. **Tabelas do Terraria:** aumentar com segurança os seis arrays
   descritos acima. Usar `TypeTables::resizedCopy` e
   `content::setTableElement` quando couber, validar tipos/limites no
   IL2CPP, manter as texturas vivas e reaplicar os dados se o jogo recriar
   uma tabela.
3. **Spawn de verdade:** expor API C# própria (nome, posição, velocidade
   e escala), comunicar-se com a thread do jogo por uma fila limitada e
   chamar `Terraria.Gore.NewGore` nativamente. Proibir chamadas diretas
   ao IL2CPP a partir do worker MonoVM.
4. **Ciclo de vida:** assegurar descarte correto ao descarregar mods,
   reiniciar o mundo e trocar sessão; não reutilizar tipos incorretos
   nem vazá-los em outro mod.
5. **Exemplos C#:** implementar no próprio
   `ExampleModCSharp` pelo menos uma gore de projétil e uma de NPC,
   com comentários explicando *quando* usar a API e *por que* os
   valores escolhidos correspondem à fonte original. Não colocar APIs
   fictícias no exemplo enquanto a ponte nativa não existir.
6. **Validação:** conferir compilação do SDK e C++, testes host de
   registro, multimoD com PNGs homônimos, ausência de JS, e no Android
   a textura, a física, o desaparecimento, troca de mundo e estabilidade
   de 60 FPS. Não declarar sucesso Android com base em compilação.

**Bloqueio de APK:** a primeira compilacao Android so pode ocorrer **depois**
de finalizar o registro, a API de spawn, os exemplos e os testes estaticos.
O APK subsequente sera exclusivamente para QA no aparelho, nao uma release.
Sem merge nem versao publica antes de validar o QA real.

## Executar os testes desta etapa (somente fonte)

Na raiz do repositório:

```bash
c++ -std=c++20 -O2 -Wall -Wextra -Werror -pedantic \
  -Iapp/src/main/cpp \
  tests/csharp_gore_discovery_test.cpp \
  app/src/main/cpp/experiments/csharp/ManagedGoreDiscovery.cpp \
  -o /tmp/bunny_gore_discovery_test
/tmp/bunny_gore_discovery_test
c++ -std=c++20 -O2 -Wall -Wextra -Werror -pedantic \
  -Iapp/src/main/cpp \
  tests/csharp_gore_catalog_test.cpp \
  -o /tmp/bunny_gore_catalog_test
/tmp/bunny_gore_catalog_test
```

Saídas esperadas: `CSHARP_GORE_DISCOVERY_PASS` e
`CSHARP_GORE_CATALOG_PASS`. Isso valida somente a descoberta e o catalogo;
nao prova o registro IL2CPP ou os efeitos no jogo.
