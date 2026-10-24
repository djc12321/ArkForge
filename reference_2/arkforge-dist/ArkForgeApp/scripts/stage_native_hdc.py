"""Stage the local TCP-only HDC client/server, built against the OHOS NDK ABI."""
import hashlib
import json
import shutil
from pathlib import Path

project = Path(__file__).resolve().parent.parent
source = Path(r'\\wsl.localhost\Ubuntu\home\djc\.cache\arkforge\hdc-ohos-ndk\libhdc_z.so')
data = source.read_bytes()
if data[:4] != b'\x7fELF' or int.from_bytes(data[18:20], 'little') != 183:
    raise RuntimeError('HDC must be an AArch64 ELF')
target = project / 'entry/libs/arm64-v8a/libhdc_z.so'
target.parent.mkdir(parents=True, exist_ok=True)
shutil.copy2(source, target)
manifest = project / 'native/hdc/staged-hdc.json'
manifest.write_text(json.dumps({'sha256': hashlib.sha256(data).hexdigest(),
    'upstreamCommit': '10e867eb30f43b605089458be20fd85e943a327d',
    'transport': 'TCP only; normal daemon authentication retained'}, indent=2), encoding='utf-8')
print('Staged AArch64 TCP-only HDC library.')
