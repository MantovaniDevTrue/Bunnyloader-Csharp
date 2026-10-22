# Desenvolver mods C# no Bunny

Contratos do [checkpoint atual](STATUS.md). Os fontes do loader e do
ExampleModBunny são a referência. O [guia rápido](PRIMEIRO-MOD.md) mostra como
criar e empacotar um mod independente. Validação é registrada por recurso.

## Primeiro mod

Use o [StarterMod](../Split/Bunny-Loader/samples/StarterModCSharp/README.md).
Ele contem apenas um material e uma receita. O gerador cria um UID novo para
cada mod, namespace e projeto `.csproj` para autocomplete/compilacao no PC.
Somente `Playable/` entra no pacote. No Android, o loader compila os `.cs`;
o usuario nao precisa de compilador externo nem de uma `.so` por mod.

Mantenha UID e nomes dos conteudos entre atualizacoes do mesmo mod. Uma entidade
usa um slot temporario; o slot nao e uma identidade adequada para save ou rede.
Itens e tiles podem ter o mesmo nome se pertencerem a categorias diferentes;
nomes repetidos dentro do mesmo dominio nativo continuam sendo recusados.

## Quando o codigo executa

| Caminho | Momento e thread | Dados e efeito |
| --- | --- | --- |
| `SetDefaults`, `SetStaticDefaults`, `AddRecipes` | Preparacao gerenciada; host instala os descritores na thread apropriada | Configuram conteudo. Nao sao callbacks livres por frame |
| `BunnyItem.Behavior`, armaduras e asas | Descritores produzidos no carregamento; policies aplicadas no jogo | Instalacao e execucao nativas, sem os adaptadores JS |
| `BunnyProjectile.OnHitNPC`, `OnKill`, `OnKillDuration` | Amostrados na preparacao para produzir policies nativas | Nao sao os eventos estaticos gerenciados de mesmo nome |
| `NomeDoProjetilMotion.TurnAtTick(int)` | 120 amostras no carregamento | Curva aplicada pelo host; nao consulta livremente o mundo em cada tick |
| `BunnyLiveEvents.OnHitNPC/OnKill` e callbacks adicionais | Eventos reais no worker Mono persistente | Dados capturados no evento; worker separado da thread do jogo |
| `BunnyPlayer.ModifyHurt`, `ModifyHitByNPC/Projectile`, `FreeDodge` | Worker Mono; o chamador aguarda a decisao ate o prazo de 5 ms | Modificadores limitados do golpe; timeout preserva o resultado anterior |
| `BunnyPlayer.OnHurt`, `OnHitByNPC/Projectile` | Notificacoes posteriores no worker | Snapshot do golpe ocorrido, sem alterar retroativamente o dano |
| `BunnyPlayer.RegisterHooks` | Uma vez por instancia gerenciada no registro do dispatcher Mono | Registra delegates BeforeHurt (`ref`), Dodge (`out`) e AfterHurt; o handle permite Unhook |
| `BunnyPlayer.ResetEffects/UpdateEquips` | Avaliados para policies e tambem pelo dispatcher de dano | Nao representam um ciclo gerenciado geral de jogador por frame |
| Demais hooks de ciclo/movimento de `BunnyPlayer` | Retornos avaliados para policies do host | Mensagens/efeitos declarados, nao execucao arbitraria no evento original |
| Helpers de combate, receitas e dados | Na chamada C# correspondente | Calculo/definicao; nao concedem acesso direto aos objetos IL2CPP |
| `BunnyVisual` | Fila de comandos gerenciados consumida pelo host | Desenho limitado ao contrato existente; nao e API de shaders |

A [referencia gerada](../Split/Bunny-Loader/docs/generated/sdk_public_reference.md)
lista a superficie realmente compilada em BunnySourceSdk. `compile_policy`
significa preparacao de descritor; `compile_sampled_policy`, amostragem para
tabela; `worker_registration`, registro de hooks no worker; `worker_sync_pre_hit`, decisao de dano; `worker_async_post_hit`,
notificacao posterior; `managed_command_queue`, comando enfileirado;
`managed_helper`, operacao C# chamada pelo proprio codigo. A presenca de um
membro publico nao implica ponte para qualquer metodo do Terraria.

## Heranca e registro explicito

Para herdar de uma base propria, consulte o
[DeclaredContentCSharp](../Split/Bunny-Loader/samples/DeclaredContentCSharp/README.md):

```csharp
[BunnyContent(BunnyContentKind.Item, "Declared Material", "Content/Items/DeclaredMaterial.png")]
public sealed class DeclaredMaterial : SharedMaterial<int>
{
    public override void SetDefaults()
    {
        base.SetDefaults();
        Rare = 1;
    }
}
```

O atributo declara categoria, nome e textura em literais que o parser nativo
pode ler antes do MonoVM. Classes abstratas e tipos genericos abertos nao sao
registrados. Cada classe concreta tem seu proprio atributo e override de
defaults. A verificacao gerenciada percorre os ancestrais e confere a categoria;
tambem recusa um `SetDefaults` que altere o nome/textura declarados.

Esta etapa exercita uma base generica de `BunnyItem` e o parser de declaracao
de `BunnyTile`. Nao generaliza todos os formatos sintaticos do C#: para as outras
categorias, o registro antecipado ainda exige defaults literais completos.
Aliases de atributos, tipos aninhados e defaults herdados sem override concreto
nao sao contratos novos desta entrega. Classes diretas existentes continuam
funcionando sem atributo. Nos itens/tiles diretos, nome e textura em
`SetDefaults` continuam exigindo literais quando nao ha declaracao explicita.

## Acessorios com efeitos nativos

O [NativeAccessoryCSharp](../Split/Bunny-Loader/samples/NativeAccessoryCSharp/README.md)
mostra um mod independente com receita, defesa e efeitos de equipamento. Com
`Accessory=true`, `Behavior.MultiplyOnEquip`, `AddOnEquip` e `SetOnEquip`
produzem uma policy nativa; nao chamam JS nem Mono por quadro. Ha cinco atributos
float (dano melee/ranged/magic/minion e MoveSpeed) e o booleano StrongBees,
com ate 16 operacoes por item. As operacoes preservam a ordem declarada.

O hook dedicado, instalado tambem para mods que so possuem BunnyItem, de
`ApplyEquipFunctional` chama o comportamento anterior e
aplica a policy apenas a itens marcados como acessorios. A tabela imutavel da
`.33` e publicada atomicamente; o callback nao usa o mutex de definicoes. A
chamada antiga em NativeContent foi centralizada nesse hook para impedir bonus
duplicados e permitir mods independentes sem outro acessorio/jogador registrado. Vaidade tem outro hook
no jogo e nao recebe estes bonus. O Terraria reinicia os atributos normalmente;
o host nao armazena estado do Player entre os ciclos. Tipos, larguras e limites
dos campos IL2CPP sao verificados antes de instalar. Um resultado float nao
finito descarta as escritas daquela policy naquele ciclo.

O campo opcional 25 do registro ITEM preserva o formato `.33`,
`campo:operacao:valor;...`. `NativeAccessoryEffectsDefinition` e o descritor gerado pelo SDK; o mod usa as APIs
tipadas, sem precisar monta-lo manualmente. Efeitos simples deixam
`BehaviorDefinition` vazio. Descritores mistos sao dados JSON lidos diretamente
por NativeItemPolicies, sem avaliacao de script ou fallback JS. Se o contrato
nativo de um efeito falhar, o lote e recusado com o motivo original.
Registros antigos de 20 a 24 campos continuam aceitos para seus recursos antigos;
SDK, compilador e host devem vir do mesmo build.

`ExampleStatAccessory`, StrongBees e a textura Back de `WaspNest` usam o caminho
nativo. O instalador tambem cobre Head/Body/Legs, Beard, asas, escudos e botas,
clone vanilla, animacao, cor do cabelo, luz/alpha e manto. Consulte o
[contrato e roteiro de independencia](csharp-native-policy-independence.md).

## Armaduras: atributos, imunidades e conjuntos

O [ArmorPoliciesCSharp](../Split/Bunny-Loader/samples/ArmorPoliciesCSharp/README.md)
contem tres pecas com receitas vanilla, sem depender do ExampleMod. Os efeitos
de `Behavior.Armor` e `BunnyArmorSet.Effects` passam ao modulo nativo
`NativeArmorPolicies`, separado de NativeBasicItems. Os sprites vestidos desse
exemplo sao instalados por NativeItemPolicies na thread do jogo, sem EquipLoader JS.

O campo opcional 26 de ITEM transporta `A1|efeitos|imunidades|head|body|legs|textoHex|efeitosDoSet|imunidadesDoSet`.
`NativeArmorDefinition` e produzido pelo SDK. Nomes de campos/operacoes, limites,
quantidades, chaves UID/Nome e texto UTF-8 sao verificados. No host, tipos,
larguras e limites dos campos IL2CPP tambem sao conferidos antes da publicacao.
A policy confere todos os destinos, overflow de inteiros, resultados float e
limites do array buffImmune antes de modificar qualquer de seus campos.
A defesa vanilla anterior ao hook continua sob responsabilidade do Terraria.

Os hooks de `GrantArmorBenefits` e `ApplyEquipFunctional` aplicam atributos da
peca depois do comportamento original. Vaidade nao passa por esses hooks.
`UpdateArmorSets` aplica o bonus apenas quando as tres posicoes funcionais
correspondem ao conjunto. O dono da declaracao precisa ser uma dessas pecas.
Nao ha estado de Player armazenado nem chamada Mono/JS por ciclo; as tabelas de
policies sao imutaveis e publicadas atomicamente. Reset/loadout continua sendo
feito pelo Terraria. Estas APIs sao policies de carregamento, nao callbacks C#
arbitrarios em cada evento do jogo.

Na thread do jogo, o registro espera os IDs das pecas e os dados de
`ArmorSetBonuses`. Usa a API nativa Add e preserva seu tooltip e lookup por peca.
Como o loader original, reaproveita o delegate Pumpkin, interceptando esse efeito
somente para os conjuntos registrados e deixando o Pumpkin vanilla intacto.
Mesmo durante falha parcial de registro, esse delegate nao concede o bonus
vanilla ao conjunto customizado. O bonus customizado espera a instalacao acabar.

A instalacao usa Array.GetValue/SetValue e gchandles para respeitar boxing e GC
em arrays de referencia ou valor. Isso ocorre na preparacao/reparo, nao no hook
por ciclo. O registro e retomavel: nao adiciona outra entrada apos Add bem-sucedido
quando a publicacao de uma linha falha. As linhas vanilla sao conservadas e
indices novos recebem linhas vazias. A troca de SetsContaining provoca reparo,
sem Add duplicado; reconstruir todo o registro All/hot-unload nao e um contrato
novo desta entrega. O processamento pendente alterna conteudos, ate oito por
passagem. O diagnostico `bl.csharp.nativeArmorPolicies()` informa fase/erro
(tambem disponivel no host C++); nao e necessario para carregar o mod. Conteudos pendentes
impedem a mensagem de carregamento completo.

Policies que so possuem efeitos nativos deixam BehaviorDefinition vazio.
Descritores mistos sao instalados pelo servico nativo, sem ModItem JS,
CSharpArmorPolicy ou callback no motor de script. SDK, compilador e host precisam
ser do mesmo build: APKs anteriores nao leem o campo 26 nem instalam este visual.

## Eventos continuos e limites

Eventos automaticos de combate usam uma fila global com 64 entradas, ate 32
vinculos de callbacks e no maximo 16 eventos pendentes por vinculo. O consumidor
alterna os vinculos e preserva a ordem dentro de cada um. A ordem global entre
projeteis distintos deixa de ser FIFO nesse modo. A distribuicao e por vinculo,
nao por mod: um mod com muitos vinculos ainda pode receber mais turnos.

O modo automatico nao desarma ao atingir 25 mil eventos. `eventLimit=0` no
diagnostico indica execucao continua; o benchmark e o caminho manual preservam
o teto finito de 25 mil. Nao ha entrega garantida quando a fila ou a cota estiver
cheia. Os contadores de admissao e os totais queued/processed destas filas
saturam no maximo Int32 representavel; nao sao IDs de entidades.
O argumento `call` legado deixa de significar um numero limitado a 25 mil.

Apos tres falhas, o vinculo principal pode ser desarmado sem interromper os
adicionais saudaveis. Pausa por mod adicional e invalidacao de tickets entre
mundos continuam ativas. Isso isola falhas de callbacks; nao isola um crash
nativo do processo. Pausar nao descarrega assemblies nem limpa campos estaticos.

## Dano sincrono e resultados atrasados

Cada chamada usa uma conclusao propria, prazo e geracao do mundo. Quando expira,
um trabalho ainda pendente e removido. Um callback que ja comecou nao e abortado;
se terminar atrasado, o resultado nao e aplicado ao golpe. Efeitos colaterais
feitos pelo proprio C# durante um callback em execucao nao podem ser desfeitos.
A troca de mundo cancela trabalhos pendentes e invalida os resultados anteriores.

`managedHurtSchedulerStatus()` expoe submissoes, conclusoes, timeouts, cancelamentos,
retornos atrasados, espera total/maxima e fila pendente. O console legado tambem
mostra esses dados em `bl.csharp.sourceLiveHitStatus().hurtScheduler`. Esse console
e diagnostico do loader; o mod nao depende dele para se inicializar. A medicao
por quadro e percentis especificos de Hurt ainda estao no plano.

## Cache e diagnostico

O cache do compilador identifica fontes com fronteiras de tamanho, SDK,
identidades MVID das referencias, compilador e Roslyn. Um hit valida o hash da
DLL e retorna `parsed=0 integrity=verified`. DLL corrompida ou manifesto incompleto
provoca recompilacao. Fontes e bytes usados no hash sao os mesmos usados no
parsing, evitando compilar uma versao diferente durante uma edicao concorrente.

O cache externo de AutoSourceRuntime usa um envelope verificado separado.
Sua chave SHA-256 inclui nomes e bytes dos fontes, identidade do mod, versao do
envelope/contrato nativo e hashes de todos os assemblies instalados no diretorio
do runtime, incluindo SDK, compilador, probe, Roslyn e referencias. Esse scan e
feito uma vez por carregamento com fontes C# validos, nunca por mod/quadro;
`Stage=RuntimeCacheIdentity TimeMs=...` mede seu custo separadamente.

Um hit tambem verifica a DLL compilada (ate 8 MiB), os tamanhos, os marcadores
e o SHA-256 das tres secoes de descritores. Arquivo antigo, truncado, corrompido,
DLL ausente ou diferente produz miss. Falhas de validacao nao publicam secoes
parciais. As escritas mantem arquivo temporario e rename; o cache continua salvo
antes da aplicacao nativa, para que uma dependencia de jogo pendente nao exija
nova compilacao. O SHA-256 C++ e portavel e nao inicializa bibliotecas crypto.

Framework obrigatorio ausente/invalido desativa o cache externo naquele lote,
permitindo que o worker siga o caminho normal de instalacao/compilacao. A proxima
inicializacao volta a avaliar o cache. A identidade e uma fotografia do framework
instalado no inicio do lote; nao ha suporte a substitui-lo durante o carregamento.
Mudancas incompativeis no contrato nativo exigem atualizar sua versao. O hash nao
identifica automaticamente todos os binarios nativos do APK. Nao presumir melhora
no tempo Android sem medir o custo completo dos dois niveis de cache.

Erros de compilacao incluem fonte/linha/coluna. O caminho de erro Hurt identifica
mod, classe e hook. Esta etapa nao implementa PDB opcional nem linhas garantidas
nas excecoes de todos os callbacks. Para os demais sistemas, conservar a causa
original no diagnostico e nao substituir erros por sucesso generico.

## Proximas capacidades

IA geral por frame sem amostragem, estado persistente de jogador/mundo,
hot-unload, registro livre de callbacks e multiplayer continuam no [plano completo](SDK-ROADMAP.md).
Nenhum exemplo deve anunciar essas capacidades antes de possuir uma bridge
funcional, testes e evidencia de comportamento no aparelho.

## IA e combate por instância de entidade

Desde a `.90.29.58`, `BunnyNPC.AI(ref BunnyNpcAiState)` e
`BunnyProjectile.AI(ref BunnyProjectileAiState)` executam no worker MonoVM,
com estado isolado por mod/tipo/instância. `OnSpawn` prepara a instância.
Os snapshots incluem slot/tipo/geração, posição/velocidade, idade e alvo;
NPC inclui vida, e projétil inclui owner. Não são objetos IL2CPP livres.

A IA é amostrada a cada seis ticks. A `.90.29.62` antecipa o primeiro tick de
projéteis recém-observados. `AI` retorna se produziu uma alteração de velocidade;
o host só aplica a resposta quando identidade/geração/owner/sessão conferem.
Curvas nativas, SourceBolt e SourceSpark conservam seu movimento prioritário,
mas continuam alimentando o ciclo C# de eventos desde a `.61`.

Desde `.59`, `BunnyNPC.OnHit(ref BunnyNpcAiState, int damage)` e
`OnKill(ref BunnyNpcAiState)` recebem eventos reais encaminhados pelo host.
NPC.OnHit observa redução de vida entre atualizações, incluindo dano contínuo;
não substitui cada fonte de dano original. `BunnyProjectile.OnHitNPC(ref
BunnyProjectileAiState, int npcIndex, int damage)` e
`OnKill(ref BunnyProjectileAiState)` usam os hooks compartilhados de combate.
São diferentes dos overloads de policy `OnHitNPC(int damage)`/`OnKill(int damage)`.

A fila é limitada e não bloqueante: eventos podem ser rejeitados antes de
existir contexto, por estado antigo ou saturação. O SDK também declara
`OnDespawn`; sua presença não comprova despacho nativo universal. Pausa/troca
de mundo não equivalem a unload completo dos assemblies ou dos estáticos.

Consulte [ExampleSlimeNPC](../Split/Bunny-Loader/samples/ExampleModCSharp/Playable/content/Content/NPCs/ExampleSlimeNPC.cs)
e [ExampleWaveBolt](../Split/Bunny-Loader/samples/ExampleModCSharp/Playable/content/Content/Projectiles/ExampleWaveBolt.cs).
O [roteiro Android](SDK-VALIDACAO.md) explica `entity-combat-status.txt`.

## Núcleo de loaders no estilo tModLoader

O lote de [Loader/LoaderManager/ModTypeLookup](csharp-content-loader.md) fornece
loaders de dados gerenciados, aliases, singletons e ciclo transacional por mod.
[Exemplo com explicações dentro do código](../Split/Bunny-Loader/samples/ContentLoaderCSharp/ExampleDefinitions.cs).
Os contratos `managed_content_registry` e `managed_content_lifecycle` na referência
gerada são chamadas C# síncronas; não são hooks IL2CPP nem atribuem IDs do Terraria.
A integração automática com o host Android permanece pendente.
[Inventário oficial de todos os arquivos de loader](TMODLOADER-LOADERS.md).
