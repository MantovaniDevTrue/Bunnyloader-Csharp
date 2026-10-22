using BunnyLoader.Source;

namespace ExampleModCSharp.Examples.Banners;

/// <summary>
/// Exemplo do registro de banners seguindo a estrutura do ModNPC no tModLoader.
/// Este arquivo e executado apenas nos testes de contrato, NAO e um item
/// jogavel enquanto os hooks nativos do BannerSystem nao estiverem ligados.
/// </summary>
public static class ExampleBannerRegistration
{
    public static void Register(BunnyBannerRegistry banners)
    {
        // As duas variantes contam para a mesma bandeira do ExampleSlimeNPC.
        // O UID permanece estavel mesmo se os tipos numericos mudarem.
        banners.Register(new BunnyBannerDefinition(
            "examplemodcsharp", "ExampleSlimeNPC", "ExampleSlimeNPC", "ExampleSlimeBanner"));
        banners.Register(new BunnyBannerDefinition(
            "examplemodcsharp", "ExampleInvasionEliteSlimeNPC", "ExampleSlimeNPC", "ExampleSlimeBanner"));
    }

    public static bool Bind(BunnyBannerRegistry banners,
        int slimeNpcType, int eliteNpcType, int slimeBannerItemType)
    {
        // Somente depois que Terraria/IL2CPP atribuir os IDs dos conteudos.
        // Nao use IDs fixos no codigo do mod.
        return banners.TryBind("examplemodcsharp", "ExampleSlimeNPC",
                   slimeNpcType, slimeNpcType, slimeBannerItemType)
            && banners.TryBind("examplemodcsharp", "ExampleInvasionEliteSlimeNPC",
                   eliteNpcType, slimeNpcType, slimeBannerItemType);
    }
}
