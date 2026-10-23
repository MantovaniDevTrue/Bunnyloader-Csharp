# Defaults clonados e recarga

`Behavior.CloneVanillaDefaults` declara uma política nativa de cópia. O host
cria uma amostra vanilla e copia os campos suportados na thread do jogo, sem
callback Mono/JavaScript por quadro. As atribuições explícitas do `BunnyItem`
substituem os valores clonados, inclusive `0`, `false` e string vazia:

```csharp
public override void SetDefaults()
{
    DisplayName = "Minha alma";
    Texture = "Content/Items/Materials/ExampleSoul";
    Behavior.CloneVanillaDefaults("SoulofSight");

    // As dimensões e o som não foram atribuídos: vêm da amostra vanilla.
    Value = BuyPrice(gold: 1);
    MaxStack = 9999;
    Rare = ItemRarityID.Pink;
    // Estas atribuições são deliberadas, apesar de serem os valores iniciais
    // do SDK. Antes, o host não distinguia ausência de atribuição e zero.
    Damage = 0;
    Melee = false;
    KnockBack = 0;
    UseSound = "";
    Defense = 0;
}
```

A política é declarativa: atribuições feitas antes de
`Behavior.CloneVanillaDefaults` também continuam sendo overrides, preservando
os mods que já usavam essa API. Os getters do SDK são valores de autoria; eles
não leem a amostra vanilla durante a compilação. Por isso, use valores explícitos
nos overrides em vez de calcular a partir de um getter que não foi atribuído.

Se uma revisão anterior atribuía `Defense = 9` e a próxima remove essa linha,
o host volta a herdar a defesa da amostra clonada. Se a próxima revisão deixar
de declarar o clone, a defesa básica volta a zero. Uma nova atribuição explícita
`Defense = 0` continua tendo precedência sobre o clone.

O protocolo opcional `C1` guarda a presença das atribuições dos 13 inteiros
básicos, sete flags, knockback e som. A defesa mantém sua coluna independente.
Registros antigos com 20–27 colunas continuam aceitos com a semântica anterior;
os novos clones podem ter 28 colunas. O mod não deve montar o protocolo à mão.

Trocar ou remover a política de um tipo revoga o snapshot anterior antes do
próximo frame. A instalação e a liberação dos handles ficam na thread do jogo;
um resultado de instalação de revisão antiga não confirma a nova revisão.
O cálculo de elegibilidade de prefixos usa os atributos nativos aplicados,
incluindo clones e overrides de NativeDefaults, depois de aplicar os defaults.

Os testes compilam três revisões reais da mesma classe com Roslyn e passam seu
wire pelo C++ de produção. Cobrem herança, overrides explícitos, recarga,
remoção durante instalação, 100 substituições antes de um frame e outro UID.
A confirmação no Android continua pendente; não foi gerado APK.

Este lote não implementa o unload global dos itens nem a restauração de todos
os campos extras de uma instância já modificada por um clone. A restauração das três flags de animação e de WingStats é descrita abaixo;
a restauração dos equipamentos declarados em Behavior.EquipTexture é descrita ao final. O unload global continua pendente. A retirada do snapshot
impede novas cópias e callbacks da política retirada; não é uma recriação geral
da instância do Terraria.

## Retirada por mod e snapshot completo

O loader agora revoga as políticas dinâmicas por UID no reset, antes de recarregar
um mod e quando o carregamento falha. Se a aplicação dos defaults falhar depois
de publicar o lote básico, o guard também retira as políticas desse UID.
No snapshot completo, tipos ausentes são retirados; um snapshot vazio retira
todos. Outros UIDs e os tipos ainda presentes conservam suas políticas.

A retirada impede novas cópias do clone e remove as consultas dinâmicas de cor,
iluminação, asas e robe. Também impede que o gancho de reinicialização registre
novamente a animação retirada. Uma instalação que já estava em execução não pode
republicar o resultado antigo. Os handles do clone ficam retidos até o pump na
thread do jogo; a thread que solicita unload não chama o coletor IL2CPP.

As seções seguintes cobrem flags, WingStats, registro de animação e equipamentos
declarativos. Defaults básicos, demais tabelas e todas as instâncias continuam
pendentes. O teste de host não comprova unload
completo no Android.

## Restauração das flags de animação e estatísticas de asas

Esta etapa acrescenta restauração por tipo de `AnimatesAsSoul`, `ItemIconPulse`,
`ItemNoGravity` e da célula inteira de `WingStats`, incluindo seus bytes não
autorados pelo mod. Por exemplo, a declaração original continua a mesma:

```csharp
// Quatro quadros, seis ticks por quadro. O loader guarda os valores nativos
// anteriores das três flags e os restaura quando esta política é retirada.
Behavior.Animate(4, 6, animatesAsSoul: true, pulse: true, noGravity: true);
```

O loader guarda o valor anterior à publicação e o último valor que escreveu.
Na retirada, só restaura se a célula ainda tiver esse último valor. Uma alteração
posterior feita por outro mod é preservada. Uma republicação explícita sobre um
valor diferente passa a usar esse valor como nova base de restauração.

A limpeza resolve o array nativo atual; não guarda ponteiros para seus elementos.
Se Terraria trocou o array por uma cópia maior, a célula atual é restaurada.
Se um array válido ficou menor e descartou o slot, o loader encerra essa limpeza
sem aumentar o array novamente. Tabela ausente ou ABI incompatível mantém a
limpeza pendente e bloqueia a instalação da próxima política desse tipo.
O diagnóstico mostra `waiting-static-retirement` e a fila inclui essa pendência.
Uma confirmação de limpeza antiga não remove uma solicitação mais recente.

A revogação dos callbacks continua imediata. A restauração nativa atende até oito
tipos com células por pump, em rotação, preservando os demais UIDs. O teste de
host verifica um lote de 17 tipos em três pumps; isso não é medição de FPS Android.

A retirada do objeto e do índice em `Main` é descrita na seção seguinte.
A seção final cobre texturas/slots declarativos. Outras tabelas de equipamento,
defaults básicos e todas as instâncias continuam pendentes.

## Registro de animação em Main

O contrato de `Behavior.Animate` também guarda o objeto anterior em
`Main.itemAnimations[type]` e verifica se o ID já estava em
`Main.itemAnimationsRegistered`. Os objetos de animação guardados têm raízes
fortes até terminar a restauração pela thread do jogo. Arrays, listas e snapshots
têm raízes temporárias durante cada leitura/chamada nativa. O exemplo segue igual:

```csharp
// O host registra a animação nativa e suas flags. Na retirada, restaura o
// objeto anterior e remove o ID somente quando esta política o acrescentou.
Behavior.Animate(4, 6, animatesAsSoul: true, pulse: true, noGravity: true);
```

O método Android `RegisterItemAnimation` faz Contains/Add na lista antes de
gravar a referência no array. Por isso, o loader retém o estado anterior antes
de invocá-lo. Se a chamada falhar depois do Add, uma nova tentativa pode concluir
o registro sem duplicar o índice, ou o unload pode remover esse Add parcial.

Na retirada, o loader encontra a posição atual do ID usando um snapshot tipado
de `List<int>.ToArray` e chama `RemoveAt`. Não escreve os campos privados da
lista. Remove primeiro o ID que acrescentou e só depois restaura a referência
com a barreira do coletor. Uma exceção após remover o ID é recuperada sem remover
outro elemento na tentativa seguinte. Se o ID já existia antes, sua posição e
seus vizinhos são preservados. Outro objeto que substituiu a animação conserva
a referência e o registro atuais; uma escrita com a mesma referência não pode
ser distinguida de uma ausência de alteração.

Array/lista ausentes, metadados incompatíveis e exceções de snapshot ou remoção
mantêm a limpeza pendente. Uma duplicação externa do ID também é rejeitada:
o loader não escolhe um índice ambíguo nem limpa o objeto sob um ID ainda
registrado. A recuperação pode continuar quando o estado nativo for válido.
Os métodos, retorno `Int32[]`, parâmetro de `RemoveAt` e array de `DrawAnimation`
são verificados por metadados antes da publicação.

Cópias dos arrays/listas são resolvidas de novo na limpeza. Se um array menor
descartar o slot que esta política criou, o ID acrescentado é retirado sem
recriar esse slot. Uma reinicialização que substitui as duas coleções estabelece
uma nova base; o gancho reaplica a animação existente, sem construir outro
`DrawAnimation`. Os testes verificam esses casos, outro UID, objetos anteriores
registrados/não registrados e falhas antes/depois das mutações. A limpeza desses
registros compartilha o orçamento de oito tipos por pump.

Isso fecha este contrato nativo no host. Não demonstra instalação nem gameplay
no Android; demais tabelas e unload global de itens continuam pendentes.
Nenhum APK foi gerado.


## Equipamentos declarativos na recarga

O loader retira as texturas publicadas por `Behavior.EquipTexture`, os mapas
`Item.headType/bodyType/legType`, as flags de cabelo, mãos e enquadramento que
escreveu e os bindings dos slots do item. O mesmo ocorre quando uma revisão
remove a declaração de equipamento. Uma nova revisão pode trocar o caminho ou
as flags sem reaproveitar a textura em cache da revisão anterior:

```csharp
// Em SetDefaults: quatro tabelas nativas de corpo usam a textura declarada.
// ShowHands respeita a diferença entre MissingHand e HidesHands no Android.
Behavior.EquipTexture(BunnyEquipKind.Body,
    "Content/Items/Armor/Vanity/ExampleRobe_Body", BunnyEquipDrawFlags.ShowHands);
// Ao remover esta declaração numa revisão, o host retira o equipamento antigo.
// Ao trocar o caminho, publica o novo asset depois de concluir a limpeza antiga.
```

O host guarda referências fortes ao asset anterior e ao publicado. Na retirada,
restaura cada tabela somente se a referência atual ainda for a que publicou;
alterações de outros mods ficam preservadas. Usa as tabelas atuais, inclusive
cópias posteriores à publicação. Um array válido menor descarta o slot sem ser
recriado. Tabela ausente, contrato incompatível ou falha de SetValue mantém a
limpeza pendente e bloqueia a próxima instalação. O refresher periódico não
reinstala o equipamento retirado. Uma escrita parcial continua reservando o
mesmo slot para a tentativa seguinte.

Bindings retirados limpam o slot em instâncias reaplicadas somente se ainda
contiverem o slot publicado. Valores diferentes são preservados. Essa limpeza
solicita a atualização de instâncias já usada pelo host; não prova que todas as
instâncias possíveis do Terraria foram enumeradas.

Os contadores de slots são marcas de alocação compartilhadas: não diminuem nem
reciclam slots, porque outros mods podem ter alocado depois. Muitas recargas
podem esgotar o limite nativo de slots; o loader mantém a instalação pendente
com diagnóstico, sem truncar o valor. O adapter legado BunnyAccessory de
Shield/Shoes continua separado deste contrato de retirada por UID.

A limpeza libera as raízes do coletor na thread do jogo. Não destrói à força o
objeto Unity: outras referências podem usá-lo; descarte global de recursos e
memória GPU continua pendente. Igualdade da mesma referência/valor não permite
distinguir uma escrita externa idêntica. Os testes de host cobrem restauração,
referência anterior, interferência externa, tabela copiada/ausente/encurtada,
falha de escrita, troca de caminho, snapshot sem equipamento e ausência de
reinstalação após 250 pumps. Android, unload global e o adapter legado ainda
exigem validação própria. Nenhum APK foi gerado.
