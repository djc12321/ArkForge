"""Add a hash-bound SDK resource/native cache to a credential-free source ZIP.

The imported ArkTS is always compiled on the device. Native bridge libraries are
explicitly cached initial SDK build products, never called device C++ compilation.
"""
import hashlib
import json
import sys
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

project, source_zip, resource_map, output = map(Path, sys.argv[1:])
cache = output.with_suffix('.resources.zip')
hap = project / 'entry/build/default/outputs/default/entry-default-unsigned.hap'
with ZipFile(hap) as source, ZipFile(cache, 'w', ZIP_DEFLATED) as target:
    for name in source.namelist():
        if name.startswith(('ets/', 'libs/x86_64/')) or name.endswith(('.abc', '.an')):
            continue
        target.writestr(name, source.read(name))
with ZipFile(cache) as archive:
    assert not any(name.endswith('.abc') for name in archive.namelist())
    assert 'libs/arm64-v8a/libentry.so' in archive.namelist()
    assert json.loads(archive.read('module.json'))['app']['bundleName'] == 'com.multismlab.app'

roots = ['AppScope', 'entry/src/main/resources', 'entry/src/main/cpp',
         'entry/src/main/libs/arm64-v8a', 'tools/micropython-embed',
         'third_party/ngspice/src/include', 'third_party/cpython/Include']
paths = [project / name for name in ['build-profile.json5', 'entry/build-profile.json5',
    'entry/src/main/module.json5', 'entry/oh-package.json5', 'oh-package.json5']]
for root in roots:
    paths.extend(p for p in (project / root).rglob('*') if p.is_file() and '.git' not in p.parts)
inputs = []
with ZipFile(source_zip) as source:
    names = set(source.namelist())
    for path in sorted(set(paths)):
        rel = path.relative_to(project).as_posix()
        data = path.read_bytes()
        if rel not in names or source.read(rel) != data:
            raise RuntimeError('Source ZIP does not match current baseline inputs: ' + rel)
        inputs.append(dict(path=rel, sha256=hashlib.sha256(data).hexdigest()))
    manifest = dict(module='entry', bundleName='com.multismlab.app', abilityName='EntryAbility',
        kind='offline-incremental-resources-native-cache', files=inputs,
        cacheSha256=hashlib.sha256(cache.read_bytes()).hexdigest(),
        resources=json.loads(resource_map.read_text(encoding='utf8')))
    with ZipFile(output, 'w', ZIP_DEFLATED) as target:
        for name in source.namelist():
            target.writestr(name, source.read(name))
        target.writestr('.arkforge/offline.json', json.dumps(manifest, ensure_ascii=False))
        target.write(cache, '.arkforge/resources.zip')
output.with_suffix('.manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf8')
print(json.dumps(dict(output=str(output), bytes=output.stat().st_size,
    cacheInputs=len(inputs), cacheContainsAbc=False, sha256=hashlib.sha256(output.read_bytes()).hexdigest())))
