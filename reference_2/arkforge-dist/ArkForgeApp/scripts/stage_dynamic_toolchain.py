"""Stage verified ARM64 compiler plus an explicit, code-free resource build cache."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
from zipfile import ZipFile, ZIP_DEFLATED

project = Path(__file__).resolve().parent.parent
library = Path(r'\\wsl.localhost\Ubuntu\home\djc\.cache\arkforge\dynamic-src\out\device-arm64\arkcompiler\common\libarkforge_dynamic.so')
data = library.read_bytes()
assert data[:4] == b'\x7fELF' and int.from_bytes(data[18:20], 'little') == 183
readelf = r'C:\Program Files\Huawei\DevEco Studio\sdk\default\openharmony\native\llvm\bin\llvm-readelf.exe'
needed = subprocess.run([readelf, '-d', str(library)], capture_output=True, text=True, check=True).stdout
print('\n'.join(line for line in needed.splitlines() if 'NEEDED' in line))
shutil.copy2(library, project / 'entry/libs/arm64-v8a/libarkforge_dynamic.so')
shutil.copy2(Path(r'\\wsl.localhost\Ubuntu\home\djc\.cache\arkforge\signer-ohos-ndk\libsigntool.so'),
             project / 'entry/libs/arm64-v8a/libsigntool.so')
raw = project / 'entry/src/main/resources/rawfile/backend'
fixture = project / 'harmony_demo/build/default/outputs/default/harmony_demo-default-unsigned.hap'
with ZipFile(fixture) as source, ZipFile(raw / 'harmony-resource-cache.zip', 'w', ZIP_DEFLATED) as target:
    for name in source.namelist():
        if name.startswith('ets/') or name.endswith(('.abc','.so','.an')):
            continue
        target.writestr(name, source.read(name))
paths = [project / 'AppScope/app.json5', project / 'harmony_demo/src/main/module.json5']
paths += [p for p in (project / 'AppScope/resources').rglob('*') if p.is_file()]
paths += [p for p in (project / 'harmony_demo/src/main/resources').rglob('*') if p.is_file()]
manifest = {'module':'harmony_demo', 'compilerSha256': hashlib.sha256(data).hexdigest(),
            'kind': 'incremental-resource-cache',
            'files':[{'path':p.relative_to(project).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(paths)]}
(raw / 'harmony-resource-cache.json').write_text(json.dumps(manifest, indent=2),encoding='utf-8')
print('Staged dynamic compiler; resource cache contains no compiled application code.')
