"""Stage only verified AArch64 ELF compiler dependencies and matching compiler data."""
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

project = Path(__file__).resolve().parent.parent
build = Path(r'\\wsl.localhost\Ubuntu\home\djc\.cache\arkforge\build-es2panda-arm64-ndk')
readelf = Path(r'C:\Program Files\Huawei\DevEco Studio\sdk\default\openharmony\native\llvm\bin\llvm-readelf.exe')
libs = project / 'entry/libs/arm64-v8a'
libs.mkdir(parents=True, exist_ok=True)
available = {p.name: p for p in (build / 'lib').glob('*.so')}
pending = ['libes2panda-public.so']
staged = []
while pending:
    name = pending.pop()
    if name in [p['name'] for p in staged]:
        continue
    source = available[name]
    data = source.read_bytes()
    if data[:4] != b'\x7fELF' or int.from_bytes(data[18:20], 'little') != 183:
        raise RuntimeError('Refusing non-AArch64 compiler library: ' + name)
    dynamic = subprocess.run([str(readelf), '-d', str(source)], capture_output=True,
                             encoding='utf-8', check=True).stdout
    needed = re.findall(r'\(NEEDED\).*?\[(.*?)\]', dynamic)
    for dependency in needed:
        if dependency in available:
            pending.append(dependency)
        elif dependency not in ('libc.so', 'libm.so', 'libdl.so', 'libpthread.so', 'librt.so',
                                'libc++_shared.so', 'libunwind.so', 'ld-musl-aarch64.so.1'):
            raise RuntimeError('Missing compiler dependency: ' + dependency)
    shutil.copy2(source, libs / name)
    staged.append({'name': name, 'sha256': hashlib.sha256(data).hexdigest(), 'needed': needed})
generated = project / 'native/compiler_include'
generated.mkdir(parents=True, exist_ok=True)
header = Path(r'\\wsl.localhost\Ubuntu\opt\arkforge\src\ets_frontend\ets2panda\public\es2panda_lib.h')
shutil.copy2(header, generated / 'es2panda_lib.h')
shutil.copytree(build / 'tools/es2panda/generated/es2panda_lib',
                generated / 'generated/es2panda_lib', dirs_exist_ok=True)
old = project.parents[2] / 'reference/HarmonyPlayground'
shutil.copy2(old / 'native/es2abc_napi.c', project / 'native/es2abc_napi.c')
data_dir = project / 'entry/src/main/resources/rawfile/compiler'
data_dir.mkdir(parents=True, exist_ok=True)
host = old / 'build_out/reproduced-linux-x86_64'
shutil.copy2(host / 'etsstdlib.abc', data_dir / 'etsstdlib.abc')
config = json.loads((host / 'arktsconfig.json').read_text(encoding='utf-8'))
dependencies = config['compilerOptions']['dependencies']
dependencies = {name: {'path': '__COMPILER_DIR__/etsstdlib.abc'} for name, value in dependencies.items()
                if 'etsstdlib.abc' in value.get('path', '')}
config = {'compilerOptions': {'baseUrl': '__COMPILER_DIR__',
          'cacheDir': '__COMPILER_DIR__/decls', 'paths': {}, 'dependencies': dependencies}}
(data_dir / 'arktsconfig.template.json').write_text(json.dumps(config, indent=2), encoding='utf-8')
(generated / 'staged-compiler.json').write_text(json.dumps(staged, indent=2), encoding='utf-8')
print('Staged AArch64 libraries:', ', '.join(p['name'] for p in staged))
