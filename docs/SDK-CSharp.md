# Desenvolver mods C# no Bunny

Esta branch parte da .90.29.31 e acrescenta uma etapa experimental do SDK e do
runtime. Os fontes do ExampleModBunny e do loader original sao a referencia.
Os novos recursos ainda precisam de validacao no Android.

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
| `BunnyItem.Behavior`, armaduras e asas | Descritores produzidos no carregamento; policies aplicadas no jogo | Alguns caminhos ainda usam os carregadores JS existentes |
| `BunnyProjectile.OnHitNPC`, `OnKill`, `OnKillDuration` | Amostrados na preparacao para produzir policies nativas | Nao sao os eventos estaticos gerenciados de mesmo nome |
| `NomeDoProjetilMotion.TurnAtTick(int)` | 120 amostras no carregamento | Curva aplicada pelo host; nao consulta livremente o mundo em cada tick |
| `BunnyLiveEvents.OnHitNPC/OnKill` e callbacks adicionais | Eventos reais no worker Mono persistente | Dados capturados no evento; worker separado da thread do jogo |
| `BunnyPlayer.ModifyHurt`, `ModifyHitByNPC/Projectile`, `FreeDodge` | Worker Mono; o chamador aguarda a decisao ate o prazo de 5 ms | Modificadores limitados do golpe; timeout preserva o resultado anterior |
| `BunnyPlayer.OnHurt`, `OnHitByNPC/Projectile` | Notificacoes posteriores no worker | Snapshot do golpe ocorrido, sem alterar retroativamente o dano |
| `BunnyPlayer.ResetEffects/UpdateEquips` | Avaliados para policies e tambem pelo dispatcher de dano | Nao representam um ciclo gerenciado geral de jogador por frame |
| Demais hooks de ciclo/movimento de `BunnyPlayer` | Retornos avaliados para policies do host | Mensagens/efeitos declarados, nao execucao arbitraria no evento original |
| Helpers de combate, receitas e dados | Na chamada C# correspondente | Calculo/definicao; nao concedem acesso direto aos objetos IL2CPP |
| `BunnyVisual` | Fila de comandos gerenciados consumida pelo host | Desenho limitado ao contrato existente; nao e API de shaders |

A [referencia gerada](../Split/Bunny-Loader/docs/generated/sdk_public_reference.md)
lista a superficie realmente compilada em BunnySourceSdk. `compile_policy`
significa preparacao de descritor; `compile_sampled_policy`, amostragem para
tabela; `worker_sync_pre_hit`, decisao de dano; `worker_async_post_hit`,
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

Essa verificacao cobre o cache de assembly do compilador. O cache externo de
descritores em AutoSourceRuntime e outro nivel, preservado nesta etapa; sua
identidade de SDK/host e sua integridade ainda precisam ser reforcadas.
Nao presumir melhora no tempo Android sem medir os dois niveis separadamente.

Erros de compilacao incluem fonte/linha/coluna. O caminho de erro Hurt identifica
mod, classe e hook. Esta etapa nao implementa PDB opcional nem linhas garantidas
nas excecoes de todos os callbacks. Para os demais sistemas, conservar a causa
original no diagnostico e nao substituir erros por sucesso generico.

## Proximas capacidades

IA gerenciada geral por entidade/frame, estado persistente de jogador/mundo,
hot-unload, registro livre de callbacks, multiplayer e dependencia zero de JS
em todos os equipamentos continuam no [plano completo](SDK-ROADMAP.md).
Nenhum exemplo deve anunciar essas capacidades antes de possuir uma bridge
funcional, testes e evidencia de comportamento no aparelho.
