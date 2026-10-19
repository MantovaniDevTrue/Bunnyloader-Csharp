"""Verify required assets in the final ZIP, using executed C# defaults from CI."""
import argparse
import json
import re
import struct
import zlib
from pathlib import PurePosixPath, Path
from zipfile import ZipFile


def png_valid(data):
    if not data.startswith(b'\x89PNG\r\n\x1a\n'):
        return False
    offset=8
    seen_header=seen_pixels=False
    while offset+12<=len(data):
        size=struct.unpack_from('>I',data,offset)[0]
        end=offset+12+size
        if end>len(data): return False
        kind=data[offset+4:offset+8]
        payload=data[offset+8:offset+8+size]
        if zlib.crc32(kind+payload)!=struct.unpack_from('>I',data,offset+8+size)[0]: return False
        if not seen_header:
            if kind!=b'IHDR' or size!=13 or not all(struct.unpack_from('>II',payload)): return False
            seen_header=True
        if kind==b'IDAT': seen_pixels=True
        # Original Bunny textures can carry data after IEND; decoders ignore it.
        if kind==b'IEND': return size==0 and seen_pixels
        offset=end
    return False


def validate(package, requirements=None):
    with ZipFile(package) as archive:
        names=archive.namelist()
        if len(names)!=len(set(names)): raise ValueError('DuplicateArchiveEntry')
        files=set(names)
        manifest=json.loads(archive.read('manifest.json'))
        mod=manifest.get('uid',manifest.get('id','unknown'))
        requested=[]
        # Literal references also protect packaging tools used before Roslyn runs.
        for name in sorted(files):
            if name.startswith('content/') and name.endswith('.cs'):
                source=archive.read(name).decode('utf-8-sig')
                declaration=re.search(r'\bclass\s+(\w+)',source)
                for sound in re.finditer(r'\bnew\s+BunnySoundStyle\s*\(\s*"([^"\r\n]+)"',source):
                    requested.append(dict(path=sound[1],file=name,className=declaration[1] if declaration else "SourceLiteral",kind="audio"))
                for match in re.finditer(r'\b(?:VisualTexture|Texture)\s*=\s*"([^"\r\n]+)"|\bEquipTexture\s*\([^,]+,\s*"([^"\r\n]+)"',source):
                    requested.append(dict(path=match.group(1) or match.group(2),file=name,
                                          className=declaration.group(1) if declaration else 'SourceLiteral'))
        if requirements is not None:
            requested+=requirements
        errors=[]
        checked=set()
        for entry in requested:
            relative=entry['path']
            audio=entry.get('kind')=='audio' or PurePosixPath(relative).suffix.lower() in ('.wav','.ogg','.mp3')
            if not audio and not relative.endswith('.png'): relative+='.png'
            path=PurePosixPath(relative)
            reason=''
            if path.is_absolute() or '..' in path.parts or '\\' in relative: reason='UnsafeAssetPath'
            member='content/'+relative
            if not reason and member not in files: reason='AssetMissing'
            if not reason and member not in checked:
                data=archive.read(member)
                if audio:
                    ext=path.suffix.lower()
                    valid=(ext=='.ogg' and len(data)>27 and data.startswith(b'OggS')) or (ext=='.wav' and len(data)>44 and data[:4]==b'RIFF' and data[8:12]==b'WAVE') or (ext=='.mp3' and len(data)>10 and (data.startswith(b'ID3') or data[0]==255 and data[1]&224==224))
                    if not valid: reason='InvalidAudioHeader'
                elif not png_valid(data): reason='InvalidPng'
            checked.add(member)
            if reason:
                errors.append('[CSharp][ERROR][PackageAsset] Mod='+mod+' Class='+entry['className']+
                    ' File='+entry.get('file','CompiledDefaults')+' Path='+member+
                    ' Stage=VerifyFinalArchive Expected='+('ExistingAudioHeader' if audio else 'ExistingValidPng')+' Actual='+reason+
                    ' Reason='+reason+' Result=PublicationRejected')
        if errors: raise ValueError('\n'.join(errors))
        if not requested: raise ValueError('NoCSharpAssetReferences')
        return dict(mod=mod,version=manifest['version'],assets=len(checked),
                    sourceFiles=sum(name.endswith('.cs') for name in files),
                    compiledDefaultsChecked=requirements is not None)


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('package',type=Path)
    parser.add_argument('--compiled-assets',type=Path)
    args=parser.parse_args()
    requirements=json.loads(args.compiled_assets.read_text()) if args.compiled_assets else None
    print('CSHARP_FINAL_ARCHIVE_ASSETS_PASS '+json.dumps(validate(args.package,requirements)))
