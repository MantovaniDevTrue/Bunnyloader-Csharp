using System;
using System.Collections.Generic;
using BunnyLoader.Source;

namespace ExampleModCSharp.Examples.Hairs;

/// <summary>
/// Baseado no ExampleHair do ExampleModBunny e na assinatura ModHair do tModLoader.
/// Este exemplo está em Examples/, NÃO em Playable/: ele só deverá ser ativado
/// depois que a instalação de texturas, a interface, save e rede forem nativas.
/// Copiar ExampleHair.png e ExampleHair_Alt.png do ExampleModBunny para
/// Content/Hairs/ no projeto que for carregar este tipo.
/// </summary>
public sealed class ExampleHair : ModHair
{
    public override string Texture => "Content/Hairs/ExampleHair";

    // Quando um chapéu cobre o cabelo, esta textura alternativa é usada.
    public override string AltTexture => "Content/Hairs/ExampleHair_Alt";

    // O jogador já pode escolher este penteado quando cria o personagem.
    public override bool AvailableDuringCharacterCreation => true;

    // Apenas uma sugestão para o seletor aleatório; não limita o jogador.
    public override HairGender RandomizedCharacterCreationGender => HairGender.Unspecified;

    // No Stylist, condições devem consultar progresso REAL do mundo.
    // Este exemplo é livre no Cabeleireiro e por isso retorna nenhuma condição.
    public override IEnumerable<Condition> GetUnlockConditions() => Array.Empty<Condition>();
}

/// <summary>
/// Exemplo de registro gerenciado para ferramentas e testes.
/// O callback existe para verificar os arquivos no pacote do mod.
/// Isto NÃO substitui a futura instalação na thread IL2CPP.
/// </summary>
public static class ExampleHairRegistration
{
    public static HairDefinition Register(HairLoader hairs, Func<string, bool> hasAsset)
    {
        return hairs.Register("examplemodcsharp", new ExampleHair(), hasAsset);
    }
}
