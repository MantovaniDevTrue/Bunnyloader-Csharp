class CSharpWingPolicy {
    static Validate(b,flags,equips) {
        if(b.wings==null)return;
        const w=b.wings;
        if(!(flags&32)||!equips.has('Wings'))throw new Error('WingPolicy Expected=AccessoryAndWingsTexture Actual=Missing');
        if(!Array.isArray(w.stats)||w.stats.length!==3||!Number.isInteger(w.stats[0])||w.stats[0]<1||w.stats[0]>6000||
            w.stats.slice(1).some(v=>!Number.isFinite(v)||v<0||v>100)||
            !Array.isArray(w.vertical)||w.vertical.length!==5||w.vertical.some(v=>!Number.isFinite(v)||v<0||v>10))
            throw new Error('WingPolicy: valores fora do contrato');
    }
    static Install(type,w) {
        const slot=EquipLoader.CSharpItemSlot(type,'Wings');
        const stats=Terraria.ID.ArmorIDs.Wing.Sets.Stats[slot];
        if(!stats)throw new Error('WingPolicy Stage=Install Slot='+slot+' Field=WingStats Actual=Missing');
        stats.FlyTime=w.stats[0];stats.AccRunSpeedOverride=w.stats[1];stats.AccRunAccelerationMult=w.stats[2];
        stats.HasDownHoverStats=false;stats.DownHoverSpeedOverride=-1;stats.DownHoverAccelerationMult=1;
    }
}
