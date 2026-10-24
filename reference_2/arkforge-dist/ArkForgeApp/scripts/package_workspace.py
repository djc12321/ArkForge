"""Bundle an editable, credential-free copy of the real two-module DevEco project."""
import json
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

root = Path(__file__).resolve().parent.parent
output = root / 'entry/src/main/resources/rawfile/HarmonyProject.zip'
output.parent.mkdir(parents=True, exist_ok=True)
sources = []
for directory in ('AppScope', 'entry/src', 'harmony_demo', 'hvigor', 'licenses'):
    for path in (root / directory).rglob('*'):
        rel = path.relative_to(root)
        if not path.is_file() or any(x in rel.parts for x in ('build', 'oh_modules', '.cxx', 'rawfile')):
            continue
        sources.append(path)
for name in ('hvigorfile.ts', 'oh-package.json5', 'entry/build-profile.json5',
             'entry/hvigorfile.ts', 'entry/oh-package.json5', 'native/CMakeLists.txt',
             'native/jsvm_bridge.c', 'README.md', 'THIRD_PARTY_NOTICES.md'):
    sources.append(root / name)
for path in (root / 'native/quickjs').iterdir():
    if path.is_file() and (path.suffix in ('.h', '.c') or path.name == 'LICENSE'):
        sources.append(path)
profile = json.loads((root / 'build-profile.json5').read_text(encoding='utf-8'))
profile['app'].pop('signingConfigs', None)
for product in profile['app']['products']:
    product.pop('signingConfig', None)
with ZipFile(output, 'w', ZIP_DEFLATED) as archive:
    for path in sorted(set(sources)):
        archive.write(path, str(path.relative_to(root)).replace('\\', '/'))
    archive.writestr('build-profile.json5', json.dumps(profile, indent=2))
print('Packaged real HarmonyOS project:', output, output.stat().st_size, 'bytes')
