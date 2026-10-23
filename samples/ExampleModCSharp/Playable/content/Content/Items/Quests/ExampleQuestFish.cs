using BunnyLoader.Source;

namespace ExampleModCSharp;

// O AnglerQuestLoader publica este item nas tabelas nativas do Pescador e
// retira lista, flags e delegates quando o mod sai. Veja Documentation/THREE_LOADERS.md.
public sealed class ExampleQuestFish : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName = "Example Upside-down Fish";
        Texture = "Content/Items/Quests/ExampleQuestFish.png";
        DefaultToQuestFish();
        QuestAvailableInPreHardmode = false;
        QuestAvailableInHardmode = true;
        QuestDescription = "I've heard stories of a fish that swims upside-down. Supposedly you have to stand upside-down yourself to even find one. One of those would go great on my ceiling. Go fetch!";
        QuestCatchLocation = "Caught anywhere while standing upside-down.";
        QuestDescriptionTranslations["pt-BR"] = "Ouvi histórias de um peixe que nada de ponta-cabeça. Dizem que você mesmo precisa estar de ponta-cabeça para achar um. Um desses ficaria ótimo no meu teto. Vai buscar!";
        QuestCatchLocationTranslations["pt-BR"] = "Pescado em qualquer lugar, de ponta-cabeça.";
    }
}
