using System;
using BunnyLoader.Source;

namespace ExampleModCSharp;

[BunnyName("en-US", "Example Shotgun")]
[BunnyName("pt-BR", "Example Escopeta")]
public sealed class ExampleShotgun : BunnyItem
{
    public override void SetDefaults()
    {
        DisplayName="Example Shotgun";
        Texture="Content/Items/Weapons/Ranged/ExampleShotgun.png";
        Width=44;Height=18;Rare=2;Value=BuyPrice(gold:10);
        UseTime=55;UseAnimation=55;UseStyle=5;AutoReuse=true;UseSound="Item36";
        Damage=10;KnockBack=6;NoMelee=true;
        NativeDefaults.SetBool("ranged",true).SetId("useAmmo","AmmoID","Bullet")
            .SetId("shoot","ProjectileID","PurificationPowder").SetFloat("shootSpeed",10);
        Holdout=new BunnyHoldout(-2,-2);
    }

    public override bool OnShoot(BunnyShootContext context,BunnyShot shot)
    {
        // O Terraria já escolheu a munição: copiar o snapshot preserva tipo, dano, dono e fonte.
        // Oito spawns pertencem a um único uso; o motor continua responsável por consumir a munição.
        for(int i=0;i<8;i++)
        {
            float angle=(context.Random.NextSingle()*2-1)*(MathF.PI/12);
            float speed=1-context.Random.NextSingle()*0.3f;
            float cos=MathF.Cos(angle),sin=MathF.Sin(angle);
            var pellet=shot;
            pellet.VelocityX=(shot.VelocityX*cos-shot.VelocityY*sin)*speed;
            pellet.VelocityY=(shot.VelocityX*sin+shot.VelocityY*cos)*speed;
            context.Spawn(pellet);
        }
        // Só os oito pedidos são aplicados; true acrescentaria também o tiro original.
        return false;
    }
}
