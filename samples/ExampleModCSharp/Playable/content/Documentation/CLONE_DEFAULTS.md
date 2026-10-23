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
texturas, slots e demais tabelas estáticas de equipamento continuam pendentes. A retirada do snapshot
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

A extensão abaixo restaura três flags e WingStats. A retirada de texturas/slots,
demais tabelas, registro de animação já existente no Terraria, defaults básicos
e todas as instâncias continua pendente. O teste de host não comprova unload
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

**Ainda pendente:** retirar o objeto e o índice já registrados em
`Main.itemAnimations`/`itemAnimationsRegistered`, restaurar texturas/slots e as
outras tabelas de equipamento, defaults básicos e todas as instâncias. As três
flags restauradas não equivalem à retirada completa do registro de animação.
