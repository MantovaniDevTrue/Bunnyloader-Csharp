# Serviço nativo de SurfaceBackground — continuidade de 22/10

Implementação sobre `eb6c44b4` em `content/backgrounds/SurfaceBackgrounds.*` e
`SurfaceProperties.h`, relativa a `app/src/main/cpp`. O loader C# permanece
**parcial**: não há API pública, exemplo C# ou bridge MonoVM/QuickJS novos nesta
etapa. O JS original continua utilizando seu loader anterior. Não iniciar o SDK
C# contra este registro sem adaptar também a bridge JS, pois os slots das duas
linguagens precisam pertencer ao mesmo serviço antes de uma entrega.

## Caminho implementado

`reserve(modUid, className, language)` valida identidade, a contagem nativa e
todos os contratos antes de instalar seis detours. A reserva é idempotente por
UID/classe, recusa conflito de linguagem e não serializa IDs de sessão. O limite
de 4096 inclui os estilos vanilla; as tabelas comportam todo o intervalo. Lookup,
seleção por prioridade/peso, menu, retire e reset de mundo são serviços C++.
Os dois providers ainda são funções C++ internas; os testes não os confundem
com execução real de C# ou JavaScript.

| Hook | Comportamento |
| --- | --- |
| GetPreferredBGStyleForPlayer | Executa original uma vez; mantém a escolha vanilla no menu e arbitra floresta/selva/neve por prioridade/peso fora dele. Desempate por UID/classe independe da ordem de carregamento. |
| UpdateBGVisibility_BackLayer | Distingue override ausente de zero. Aplica o tema de menu por setter/campo verificado e chama DrawBG_ModifyBGFarBackLayerAlpha com tipos reais. |
| UpdateBGVisibility_FrontLayer | Original primeiro, callback de fades depois; valida comprimento e valores antes da cópia, e rejeita resposta de mundo/array/dono substituído. |
| DrawSurfaceBG_BackMountainsStep1 | Original primeiro, captura top/push e desenha Far nas posições deixadas pelo Terraria. |
| DrawSurfaceBG_BackMountainsStep2 | Original primeiro e Middle depois; cada chamada pede a textura ao provider, preservando animação guiada por chamadas. |
| DrawSurfaceBG_GetFogPower | Consome Step1 uma vez, desenha Close antes do original e preserva o retorno original. |

Todos os métodos precisam ter corpo, storage, retorno, parâmetros e tamanhos
compatíveis. Nullable<Int32>/Nullable<Single> só passam como agregado em GPR
quando a metadata comprova Boolean de um byte, payload exato de quatro bytes,
layout sem sobreposição, tamanho oito e alinhamento até oito. Os métodos precisam
compartilhar as mesmas classes genéricas fechadas. O wrapper transporta os bits
sem converter ausência em zero. Em ARM64 isso é um agregado não-HFA; não se usa
o ABI de dois floats nem offsets importados do PC.

Falha de binding não instala hooks. Uma falha do instalador depois de dois
detours deixa wrappers transparentes, somente chamando originais; uma nova
tentativa instala apenas os elos faltantes. Nenhuma entrada é registrada como
funcional antes de terminar a instalação. HookManager continua podendo atualizar
os ponteiros de original quando outra bridge participa da cadeia.

`SurfaceProperties` lê Int32/Single/Double pela largura real. A escrita de uma
propriedade confirma o setter void com parâmetro exato e storage correspondente.
Tabelas ainda exigem campos diretos: quando Main oferece getter, o núcleo valida
o campo de mesmo tipo em LocalUserGameState, sem publicar quatro setters que
possam falhar no meio. Color usa a classe/layout reais e campos/canais verificados;
getter e setter não são presumidos intercambiáveis com ponteiros de objetos.

`scAdj`, bgTopY, start e projeções preservam frações. O ajuste vertical tinha
sido tratado como inteiro no helper anterior; a comparação com o JS agora usa
`-11.375` para impedir regressão. Comandos internos de desenho personalizado
suportam retângulo opcional, posição, origem, rotação, escala, efeitos e profundidade
na sobrecarga verificada. Há limite de 128 sprites por callback; um comando
não pode multiplicar loops nem invocar céu arbitrariamente. Isto prepara o
callback real, mas ainda não constitui a API C# desse recurso.

O cálculo padrão de fades mantém também uma velocidade Double até a escrita
final no array Single. Um teste compara o original JS num valor imediatamente
abaixo do ponto médio entre dois floats: converter a velocidade para Single
antes da soma fazia o arredondamento seguir para o vizinho errado.

## Recursos, falhas e ciclo de vida

- Escolha de textura e callbacks ocorrem sem o mutex do registro. Um teste
  inicia outra thread, altera o registro e aguarda sua conclusão dentro do
  callback, comprovando ausência desse bloqueio entre as duas threads.
- Arrays de fades e dono ficam enraizados durante callbacks/desenho; uma nova
  instância não pode liberar os arrays que o snapshot anterior ainda referencia.
- Cada camada mantém raiz e lease da textura. Unload preserva a imagem anterior
  até que os alphas correspondentes zerem; destruir uma textura durante o desenho
  apenas agenda a destruição quando necessário. Dependência aposentada recusa
  novas escolhas e pode desenhar a imagem anterior retida durante o fade, com
  diagnóstico explícito. Recarregar o estilo refaz sua resolução de conteúdo.
- Uma chamada reentrante/concorrente de desenho preserva as camadas originais e
  informa DrawDispatcherBusy com limitação de repetição. Exceção ou resposta
  inválida de um estilo não interrompe a iteração de outros estilos.
- Troca de mundo invalida epoch, seleção de cena e Step1 pendente, preservando a
  seleção independente de menu. A saída para o menu é observada por Cheats;
  a entrada reseta em noteAutoWorldTilesLoaded. Não há segundo reset tardio em
  Cheats ao entrar, que apagaria uma ativação feita em OnEnterWorld.
  Nenhum ID novo vai ao save.
- Logs são limitados por slot/operação; erros de assinatura nullable preservam
  o detalhe do campo/tipo incompatível, sem reduzir a causa a registro falhou.

## Testes e limites da evidência

`bash tests/surface_backgrounds/run.sh` agora executa também
`native_surface_loader_test.cpp`, que utiliza todos os detours e o código de
produção, com metadata/heap controlados. Cobre registro compartilhado com gaps,
tipos/storage incompatíveis, falha/retry de instalação, originais, overrides
nullable, prioridade/peso, fades, mudanças de array/dono, seis hooks, slots de
texturas realmente desenhados, menu, mundo, callback rejeitado, comandos próprios
e unload durante transição. O oráculo JS anterior continua preservado.

`BUNNY_SURFACE_SANITIZE=1` acrescenta ASan/UBSan/divisão por zero/leaks aos dois
executáveis. O CI inclui sintaxe ARM64 da unidade nova e linkagem completa sem
APK. Não há nova medição de tempo de carregamento, GC Android, GPU ou 60 FPS.

Continuam faltando: protocolo nativo/worker com revisão e deadline; estágio
gerenciado preservando os índices existentes 11/12; SDK e inventário/cache se
mudarem; port C# integral das seis imagens e animação original; bridge QuickJS
para o registro compartilhado; execução real C#/JS juntas; QA de dispositivo.
O exemplo de superfície continua não portado na matriz dos 206 JS.

CI do código `bc5ed296`: onze workflows aprovados; os quatro jobs do combinado
[39726493557](https://github.com/MantovaniDevTrue/Bunnyloader-Csharp/actions/runs/39726493557)
incluem sanitizadores, comparações, regressões, sintaxe e linkagem ARM64, sem APK.
Evidência completa em [validation/csharp-surface-native-hooks-2026-10-22.json](validation/csharp-surface-native-hooks-2026-10-22.json).
