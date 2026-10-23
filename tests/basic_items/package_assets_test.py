"""Remove real ExampleMod assets from the final ZIP to reproduce Android failures."""
import io
import sys
import unittest
from pathlib import Path
from zipfile import ZipFile

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/mods'))
from pack import pack_bytes
from check_csharp_assets import validate


class PackageAssetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.package=pack_bytes(ROOT/'samples/ExampleModCSharp/Playable')
        with ZipFile(io.BytesIO(cls.package)) as archive:
            cls.files={name:archive.read(name) for name in archive.namelist()}

    def altered(self,omit=None,replace=None):
        out=io.BytesIO()
        with ZipFile(out,'w') as archive:
            for name,body in self.files.items():
                if name!=omit: archive.writestr(name,replace[1] if replace and name==replace[0] else body)
        out.seek(0)
        return out

    def test_real_example_package_is_self_contained(self):
        result=validate(io.BytesIO(self.package))
        self.assertGreater(result['assets'],0)
        self.assertEqual(result['sourceFiles'],sum(name.endswith('.cs') for name in self.files))

    def test_each_previously_copied_asset_is_mandatory(self):
        for relative in ('Buffs/ExampleDefenseBuff','Projectiles/SourceBolt','Items/Weapons/SourceWand',
                         'Items/Consumables/ExampleBuffPotion','Items/Accessories/ExampleFocusAccessory',
                         'NPCs/ExampleSlimeNPC','Walls/ExampleWallAdvanced','Items/Placeable/ExampleWallAdvanced'):
            member='content/Content/'+relative+'.png'
            with self.subTest(asset=member), self.assertRaises(ValueError) as error:
                validate(self.altered(omit=member))
            self.assertIn('Reason=AssetMissing',str(error.exception))
            self.assertIn('Path='+member,str(error.exception))
            self.assertIn('Result=PublicationRejected',str(error.exception))

    def test_original_gun_audio_is_required(self):
        member='content/Assets/Sounds/Items/Guns/ExampleGun.ogg'
        with self.assertRaisesRegex(ValueError,'Reason=AssetMissing'):
            validate(self.altered(omit=member))
        with self.assertRaisesRegex(ValueError,'Reason=InvalidAudioHeader'):
            validate(self.altered(replace=(member,b'not ogg')))

    def test_corrupt_png_is_not_accepted_as_an_asset(self):
        member='content/Content/Buffs/ExampleDefenseBuff.png'
        with self.assertRaisesRegex(ValueError,'Reason=InvalidPng'):
            validate(self.altered(replace=(member,b'\x89PNG\r\n\x1a\n')))

    def test_computed_defaults_are_checked_too(self):
        with self.assertRaisesRegex(ValueError,'Class=ComputedTextureBuff'):
            validate(io.BytesIO(self.package),[dict(path='Content/Buffs/MissingComputed.png',
                                                   className='ComputedTextureBuff')])

    def test_required_equipment_sprites_are_checked(self):
        member='content/Content/Items/Accessories/ExampleWings_Wings.png'
        with self.assertRaisesRegex(ValueError,'ExampleWings_Wings.png'):
            validate(self.altered(omit=member))


if __name__=='__main__': unittest.main()
