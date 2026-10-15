// Adaptador privado do host. Mods publicam fontes C# e politicas tipadas;
// o ItemLoader original executa os hooks e a bridge valida os campos IL2CPP.
class CSharpItemBehaviorLoader {
    static #installed = new Map();
    static #staged = new Map();
    static #defaultsHooked = false;
    static #baseFields = ['width','height','maxStack','value','rare','damage','crit','useTime','useAnimation','useStyle','pick','axe','hammer'];
    static Validate(entry) {
        if (!Number.isInteger(entry.type) || entry.type<FIRST_ITEM || !Array.isArray(entry.defaults) || entry.defaults.length!==14)
            throw new Error('descritor de item invalido');
        const b=JSON.parse(entry.definition);
        if (b.v!==1 || typeof b.clone!=='string' || !/^[A-Za-z0-9_]*$/.test(b.clone) || b.clone.length>72 ||
            !Array.isArray(b.equips) || b.equips.length>4 || !Array.isArray(b.effects) || b.effects.length>16 ||
            typeof b.hair!=='boolean' || typeof b.robe!=='boolean') throw new Error('contrato de comportamento invalido');
        const seen=new Set();
        for (const e of b.equips) {
            if (!Array.isArray(e) || e.length!==3 || !['Head','Body','Legs','Beard','Back','Wings'].includes(e[0]) || seen.has(e[0]) ||
                typeof e[1]!=='string' || e[1].length>200 || !/^[A-Za-z0-9_/-]+$/.test(e[1]) || e[1].startsWith('/') ||
                !Number.isInteger(e[2]) || e[2]<0 || e[2]>3 || !bl.file.exists(entry.root+e[1]+'.png'))
                throw new Error('textura/slot invalido: '+String(e));
            seen.add(e[0]);
        }
        if (b.robe && (!seen.has('Body') || !seen.has('Legs'))) throw new Error('manto exige Body e Legs');
        if(b.nativeAccessory!==undefined && typeof b.nativeAccessory!=='boolean')throw new Error("marcador nativo de acessorio invalido");
        if(b.effects.length && !(entry.defaults[13]&32))throw new Error("efeitos de acessorio exigem Accessory=true");
        for (const e of b.effects) {
            if (!Array.isArray(e) || e.length!==3) throw new Error('efeito invalido');
            if (e[0]==='strongBees' && e[1]==='set' && e[2]===true) continue;
            if (!['meleeDamage','rangedDamage','magicDamage','minionDamage','moveSpeed'].includes(e[0]) ||
                !['add','mul'].includes(e[1]) || !Number.isFinite(e[2]) || e[2]<-10 || e[2]>10 || (e[1]==='mul' && e[2]<0))
                throw new Error('efeito nao suportado: '+String(e));
        }
        if (b.animation!==null && (!Array.isArray(b.animation) || b.animation.length!==5 ||
            !Number.isInteger(b.animation[0]) || b.animation[0]<1 || b.animation[0]>128 ||
            !Number.isInteger(b.animation[1]) || b.animation[1]<1 || b.animation[1]>600 || b.animation.slice(2).some(v=>typeof v!=='boolean')))
            throw new Error('animacao invalida');
        if (b.light!==null && (!Array.isArray(b.light) || b.light.length!==4 || b.light.slice(0,3).some(v=>!Number.isFinite(v)||v<0||v>2) || typeof b.light[3]!=='boolean'))
            throw new Error('luz invalida');
        if (b.alpha!==null && (!Array.isArray(b.alpha) || b.alpha.length!==4 || b.alpha.some(v=>!Number.isInteger(v)||v<0||v>255)))
            throw new Error('cor invalida');
        if (b.clone && !Number.isInteger(Terraria.ID.ItemID[b.clone])) throw new Error('ItemID ausente: '+b.clone);
        if(b.defense!==undefined && (!Number.isInteger(b.defense)||b.defense<0||b.defense>1000))throw new Error('Defense fora do contrato');
        if(b.nativeArmor!==undefined&&typeof b.nativeArmor!=='boolean')throw new Error('marcador nativo de armadura invalido');
        b.__armor=b.nativeArmor?{effects:[],immune:[]}:CSharpArmorPolicy.Validate(b.armor);
        CSharpWingPolicy.Validate(b,entry.defaults[13],seen);
        return b;
    }
    static Sync() {
        const api=bl.csharp;
        if (!api || typeof api.pendingItemBehaviors!=='function') return;
        const failures=[];
        for (const entry of api.pendingItemBehaviors()) {
            try {
                let inst=CSharpItemBehaviorLoader.#staged.get(entry.type);
                if (!inst) {
                    const b=CSharpItemBehaviorLoader.Validate(entry);
                    if(b.armorSet&&!b.nativeArmor&&!CSharpArmorPolicy.LookupReady())continue;
                    const Class=class extends ModItem {};
                    const armor=b.__armor;
                    if(b.wings)Class.prototype.VerticalWingSpeeds=function(item,player,falling,rising,maxCan,maxAscent,constant) {
                        falling.value=b.wings.vertical[0];rising.value=b.wings.vertical[1];maxCan.value=b.wings.vertical[2];
                        maxAscent.value=b.wings.vertical[3];constant.value=b.wings.vertical[4];
                    };
                    const armorSet=b.nativeArmor?null:CSharpArmorPolicy.BindSet(entry,b.armorSet);
                    if(armor.effects.length||armor.immune.length)Class.prototype.UpdateEquip=function(item,player) {
                        CSharpArmorPolicy.Apply(armor,player);
                    };
                    if(armorSet)Class.prototype.UpdateArmorSet=function(item,player) {
                        if(CSharpArmorPolicy.Matches(armorSet,player))CSharpArmorPolicy.Apply(armorSet.policy,player);
                    };
                    Object.defineProperty(Class,'name',{value:entry.name});
                    if (b.effects.length && !b.nativeAccessory) Class.prototype.UpdateAccessory=function(item,player,vanity) {
                        if(vanity)return;
                        for(const [field,op,value] of b.effects) {
                            if(op==='set')player[field]=value;
                            else if(op==='add')player[field]+=value;
                            else player[field]*=value;
                        }
                    };
                    if (b.light) {
                        const light=Terraria.Lighting['void AddLight(Vector2 position, float r, float g, float b)'];
                        if(typeof light!=='function')throw new Error('Lighting.AddLight indisponivel');
                        Class.prototype.PostUpdate=function(item) {
                            const scale=b.light[3]?Terraria.Main.essScale:1;
                            light(item.Center,b.light[0]*scale,b.light[1]*scale,b.light[2]*scale);
                        };
                    }
                    if (b.alpha) {
                        const color=Color.new(...b.alpha);
                        Class.prototype.GetAlpha=function(){return color;};
                    }
                    if (b.robe) Class.prototype.SetMatch=function(male,equipSlot,robes) {
                        robes.value=true;equipSlot.value=EquipLoader.CSharpItemSlot(entry.type,'Legs');
                    };
                    for(const method of ['VerticalWingSpeeds','UpdateEquip','UpdateArmorSet','UpdateAccessory','PostUpdate','GetAlpha','SetMatch']) {
                        if(Class.prototype[method]!==ModItem.prototype[method])
                            Class.prototype[method]=CSharpItemBehaviorLoader.Guard(entry,b.class||entry.name,method,Class.prototype[method]);
                    }
                    inst=new Class();inst.Type=entry.type;inst.__behavior=b;inst.__descriptor=entry;inst.__armorSet=armorSet;
                    if (b.clone) {
                        const source=Terraria.Item.new();source['void .ctor()']();
                        source['void SetDefaults(int Type, ItemVariant variant)'](Terraria.ID.ItemID[b.clone],null);
                        inst.__clone=source;
                        // `in` consulta metadata sem gerar falso erro de campo opcional.
                        inst.__cloneFields=ItemLoader.CLONED_FIELDS.filter(field=>{
                            if(!(field in source))return false;
                            const descriptor=Object.getOwnPropertyDescriptor(source,field);
                            return !descriptor || descriptor.writable;
                        });
                    }
                    CSharpItemBehaviorLoader.#staged.set(entry.type,inst);
                }
                const b=inst.__behavior;
                if(b.defense!==undefined && !api.setNativeItemDefense(entry.type,b.defense))throw new Error('Defense recusada pelo host');
                for(const [kind,path,flags] of b.equips) {
                    EquipLoader.RegisterCSharpItemTexture(inst,kind,entry.root+path+'.png');
                    const slot=EquipLoader.CSharpItemSlot(entry.type,kind);
                    if(typeof api.setNativeItemEquipSlot!=='function' || !api.setNativeItemEquipSlot(entry.type,kind,slot))
                        throw new Error('bridge de slot nativo indisponivel/rejeitada item='+entry.type+' kind='+kind+' slot='+slot);
                    if(flags&1)Terraria.ID.ArmorIDs[kind].Sets.UseHairColor[slot]=true;
                    if(flags&2)Terraria.ID.ArmorIDs[kind].Sets.HidesHands[slot]=false;
                }
                if(b.wings)CSharpWingPolicy.Install(entry.type,b.wings);
                if(b.animation) {
                    inst.SetItemAnimation(b.animation[0],b.animation[1]);
                    const sets=Terraria.ID.ItemID.Sets;
                    sets.AnimatesAsSoul[entry.type]=b.animation[2];
                    sets.ItemIconPulse[entry.type]=b.animation[3];
                    sets.ItemNoGravity[entry.type]=b.animation[4];
                }
                const old=ItemLoader.ByType.get(entry.type);
                if(old && old!==inst)throw new Error('tipo ja tem outro ModItem');
                ItemLoader.ByType.set(entry.type,inst);
                ItemLoader.Hook(inst.constructor,entry.type);
                CSharpItemBehaviorLoader.#HookDefaults();
                CSharpItemBehaviorLoader.#installed.set(entry.type,inst);
                CSharpItemBehaviorLoader.#Refresh(inst);
                if(inst.__armorSet && !inst.__armorSetInstalled) {
                    CSharpArmorPolicy.InstallSet(inst.__armorSet);
                    inst.__armorSetInstalled=true;
                }
                if(!api.acknowledgeItemBehavior(entry.type,''))throw new Error('ACK nativo recusado');
                CSharpItemBehaviorLoader.#staged.delete(entry.type);
                bl.log('csharp item behavior: '+entry.name+' type='+entry.type+' instalado');
            } catch(error) {
                const detail='[CSharp][ERROR][ItemBehavior] Mod='+entry.mod+' Class='+entry.name+' File='+entry.source+' ContentId='+entry.type+
                    ' Stage=Install Result=Pending Reason='+String(error)+' Stack='+(error.stack||'');
                api.acknowledgeItemBehavior(entry.type,detail);
                Safe.Once("CSharp.ItemBehavior:"+entry.type,detail,true);
                failures.push(detail);
            }
        }
        if(failures.length)throw new Error(failures.join("\n"));
    }
    static Guard(entry,className,method,callback) {
        let failures=0;
        return function(a,b,c,d,e,f,g,h) {
            if(failures>=3)return;
            try {return callback.call(this,a,b,c,d,e,f,g,h);}
            catch(error) {
                failures++;
                const detail='[CSharp][ERROR][Callback] Mod='+entry.mod+' Class='+className+' File='+entry.source+
                    ' Method='+method+' ContentId='+entry.type+' Stage=Invoke Result='+(failures>=3?'CallbackDisabled':'Rejected')+
                    ' FailureCount='+failures+' Exception='+String(error)+' Stack='+(error.stack||'');
                bl.csharp.acknowledgeItemBehavior(entry.type,detail);
                bl.error(detail);
            }
        };
    }
    static Apply(item,inst) {
        const b=inst.__behavior, d=inst.__descriptor;
        if(inst.__clone) {
            // Igual ao CloneDefaults original, mas nao omitir um campo declarado.
            for(const field of inst.__cloneFields) {
                try {item[field]=inst.__clone[field];}
                catch(error) {throw new Error('Class='+d.name+' Method=CloneDefaults Field='+field+' Stage=NativeWrite Result=Rejected Detail='+String(error));}
            }
            for(let i=0;i<CSharpItemBehaviorLoader.#baseFields.length;i++) {
                if(i<2 && d.defaults[i]===0)continue;
                item[CSharpItemBehaviorLoader.#baseFields[i]]=d.defaults[i];
            }
            item.knockBack=d.knockBack;
            const flags=['melee','noMelee','autoReuse','useTurn','consumable','accessory','vanity'];
            for(let i=0;i<flags.length;i++)item[flags[i]]=!!(d.defaults[13]&(1<<i));
        }
        if(b.defense!==undefined)item.defense=b.defense;
        EquipLoader.Apply(item,inst.Type);
        if(b.hair) {
            const player=Terraria.Main.LocalPlayer;
            if(!player)throw new Error('jogador local indisponivel para cor do cabelo');
            item.color=player.hairColor;
        }
    }
    static #HookDefaults() {
        if(CSharpItemBehaviorLoader.#defaultsHooked)return;
        Terraria.Item['void SetDefaults(int Type, ItemVariant variant)'].hook((original,item,type,variant)=>{
            original(item,type,variant);
            const inst=CSharpItemBehaviorLoader.#installed.get(type);
            if(inst)CSharpItemBehaviorLoader.Apply(item,inst);
        },{minType:FIRST_ITEM,arg:0});
        CSharpItemBehaviorLoader.#defaultsHooked=true;
    }
    static #Refresh(inst) {
        const apply=item=>{if(item && item.type===inst.Type)CSharpItemBehaviorLoader.Apply(item,inst);};
        apply(ItemLoader.Sample(inst.Type));
        const main=Terraria.Main;
        if(main.player)for(let i=0;i<main.player.length;i++) {
            const player=main.player[i];if(!player || !player.active)continue;
            for(const field of ['inventory','armor','dye','miscEquips','miscDyes']) {
                const items=player[field];if(items)for(let j=0;j<items.length;j++)apply(items[j]);
            }
        }
        apply(main.mouseItem);
    }
}
globalThis.__blSyncCSharpItemBehaviors=()=>CSharpItemBehaviorLoader.Sync();
