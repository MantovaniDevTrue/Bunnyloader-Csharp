"""Reproducible, non-executable inventory of a user-supplied Bunny package."""
import hashlib
import json
from pathlib import PurePosixPath
from zipfile import ZipFile


def inventory(package, upstream, extract_facts):
    with ZipFile(package) as archive:
        names = archive.namelist()
        if len(names)>4096 or sum(entry.file_size for entry in archive.infolist())>64*1024*1024:
            raise ValueError('Reference package exceeds inventory capacity')
        if len(names) != len(set(names)):
            raise ValueError('Reference package has duplicate paths')
        for name in names:
            path = PurePosixPath(name)
            if path.is_absolute() or '..' in path.parts or '\\' in name:
                raise ValueError('Unsafe reference path: ' + name)
        manifest = json.loads(archive.read('manifest.json'))
        files, examples = [], []
        for name in sorted(names):
            if name.endswith('/') or not name.startswith('content/'):
                continue
            entry = archive.getinfo(name)
            if entry.file_size > 8 * 1024 * 1024:
                raise ValueError('Reference file exceeds 8 MiB: ' + name)
            body = archive.read(name)
            record = dict(path=name, size=len(body), sha256=hashlib.sha256(body).hexdigest(),
                          gitBlob=hashlib.sha1(b'blob ' + str(len(body)).encode() + b'\0' + body).hexdigest())
            files.append(record)
            if not name.endswith('.js'):
                continue
            relative = name.removeprefix('content/')
            old = upstream / 'samples/ExampleMod/content' / relative
            source = body.decode('utf-8-sig')
            comparison = ('Unchanged' if old.is_file() and old.read_text().strip() == source.strip()
                          else 'Changed' if old.is_file() else 'Added')
            examples.append(dict(id=relative, **record, comparison=comparison,
                                 facts=extract_facts(source),
                                 **({'source':source} if comparison!='Unchanged' else {})))
        return dict(schemaVersion=1, origin='User-supplied examplemod.bl',
                    archiveSha256=hashlib.sha256(package.read_bytes()).hexdigest(),
                    manifest=manifest, files=files, examples=examples)
