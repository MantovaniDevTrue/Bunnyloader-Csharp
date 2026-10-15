// Politicas C# nos hooks reais de armadura. Nenhum callback Mono por quadro.
class CSharpArmorPolicy {
    static Validate(raw) {
        const value=raw || {effects:[],immune:[]};
        if(!Array.isArray(value.effects)||value.effects.length>16||!Array.isArray(value.immune)||value.immune.length>32)
            throw new Error('ArmorPolicy: efeitos/imunidades invalidos');
        for(const e of value.effects) {
            if(!Array.isArray(e)||e.length!==3||!Number.isFinite(e[2]))throw new Error('ArmorPolicy: efeito invalido');
            const ints=['statManaMax2','maxMinions','statDefense','lifeRegen'];
            const floats=['meleeDamage','rangedDamage','magicDamage','minionDamage','moveSpeed'];
            if(e[1]==='int') {
                if(!ints.includes(e[0])||!Number.isInteger(e[2])||Math.abs(e[2])>1000)throw new Error('ArmorPolicy: inteiro invalido '+e);
            } else if(!floats.includes(e[0])||!['add','mul'].includes(e[1])||e[2]<-10||e[2]>10||(e[1]==='mul'&&e[2]<0))
                throw new Error('ArmorPolicy: atributo invalido '+e);
        }
        const ids=[];
        for(const name of value.immune) {
            if(typeof name!=='string'||!/^\w{1,72}$/.test(name))throw new Error('ArmorPolicy: BuffID invalido');
            const id=Terraria.ID.BuffID[name];
            if(!Number.isInteger(id)||id<0)throw new Error('ArmorPolicy: BuffID ausente '+name);
            ids.push(id);
        }
        return {effects:value.effects,immune:ids};
    }
    static Apply(policy,player) {
        // Conferir todos os alvos antes de modificar qualquer atributo.
        for(const e of policy.effects)if(typeof player[e[0]]!=='number'||!Number.isFinite(player[e[0]]))
            throw new Error('ArmorPolicy Field='+e[0]+' Expected=FiniteNativeNumber Actual='+String(player[e[0]]));
        if(policy.immune.length && !player.buffImmune)throw new Error('ArmorPolicy Field=buffImmune Actual=Missing');
        for(const id of policy.immune)if(id>=player.buffImmune.length)throw new Error('ArmorPolicy Field=buffImmune BuffId='+id+' Expected=InBounds Actual='+player.buffImmune.length);
        for(const [field,op,value] of policy.effects) {
            if(op==='mul')player[field]*=value;else player[field]+=value;
        }
        for(const id of policy.immune)player.buffImmune[id]=true;
    }
    static BindSet(entry,raw) {
        if(!raw)return null;
        if(typeof raw.text!=='string'||!raw.text.length||raw.text.length>512||/[\x00-\x1f\x7f]/.test(raw.text))throw new Error('ArmorSet: texto invalido');
        const ids=['head','body','legs'].map(part=>{
            const key=raw[part];
            if(typeof key!=='string'||!key.length||key.length>160||!/^[A-Za-z0-9_/-]+$/.test(key))throw new Error('ArmorSet: chave invalida '+part);
            const qualified=key.includes('/')?key:entry.mod+'/'+key;
            const id=bl.csharp.resolveNativeItemKey(qualified);
            if(!Number.isInteger(id)||id<FIRST_ITEM)throw new Error('ArmorSet Part='+part+' Key='+qualified+' Expected=RegisteredItem Actual='+id);
            return id;
        });
        return {ids,text:raw.text,policy:CSharpArmorPolicy.Validate(raw.effects)};
    }
    static InstallSet(set) {
        const lookup=Terraria.DataStructures.ArmorSetBonuses.SetsContaining;
        if(!lookup)throw new Error('ArmorSet Stage=Install Field=SetsContaining Actual=Missing Result=Pending');
        ArmorSetLoader.CreateArmorSet(...set.ids,set.text);
    }
    static Matches(set,player) {
        return player.armor && set.ids.every((id,i)=>player.armor[i]&&player.armor[i].type===id);
    }
}
