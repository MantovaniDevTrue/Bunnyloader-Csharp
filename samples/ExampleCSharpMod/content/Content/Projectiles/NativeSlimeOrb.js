export class NativeSlimeOrb extends ModProjectile {
    SetDefaults() {
        this.Projectile.width = 12;
        this.Projectile.height = 12;
        this.Projectile.aiStyle = 1;
        this.Projectile.friendly = false;
        this.Projectile.hostile = true;
        this.Projectile.penetrate = 1;
        this.Projectile.timeLeft = 160;
        this.Projectile.ignoreWater = true;
        this.Projectile.tileCollide = true;
    }
}
