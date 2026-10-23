namespace BunnyLoader.Source;

/// <summary>
/// Valores públicos de Item.rare, com os mesmos nomes do tModLoader.
/// Esta é uma API própria do Bunny: a classe Terraria.ID.ItemRarityID não
/// existe no Terraria Android. Não requer JavaScript nem assemblies do PC.
/// Raridades Expert/Master não substituem as flags correspondentes do item.
/// </summary>
public static class ItemRarityID
{
    public const int Master = -13, Expert = -12, Quest = -11, Gray = -1;
    public const int White = 0, Blue = 1, Green = 2, Orange = 3;
    public const int LightRed = 4, Pink = 5, LightPurple = 6, Lime = 7;
    public const int Yellow = 8, Cyan = 9, Red = 10, Purple = 11, Count = 12;
}
