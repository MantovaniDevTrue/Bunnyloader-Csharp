namespace BunnyLoader.Source;

public abstract class BunnyGlobalItem
{
    public virtual bool AppliesTo(BunnyPrefixItemSnapshot item) => true;
    public virtual bool AllowPrefix(BunnyPrefixItemSnapshot item, int prefix) => true;
    public virtual int ChoosePrefix(BunnyPrefixItemSnapshot item) => -1;
    public virtual bool? DecidePrefixChance(BunnyPrefixItemSnapshot item, int request) => null;
    public virtual void ApplyPrefix(BunnyPrefixItemSnapshot item, int prefix,
        ref BunnyPrefixApplyStats effects) { }
}
