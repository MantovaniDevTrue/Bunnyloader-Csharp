export class NativeHomingBolt extends ModProjectile {
    SetDefaults() {
        this.Projectile.width = 8;
        this.Projectile.height = 8;
        this.Projectile.aiStyle = 1;
        this.Projectile.friendly = true;
        this.Projectile.hostile = false;
        this.Projectile.ranged = true;
        this.Projectile.penetrate = 1;
        this.Projectile.timeLeft = 420;
        this.Projectile.ignoreWater = true;
        this.Projectile.tileCollide = true;
    }
}
