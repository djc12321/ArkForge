"""Stage the in-process OHOS signer and a public signed HAP verification fixture."""
import hashlib
import json
import shutil
from pathlib import Path

project = Path(__file__).resolve().parent.parent
source = Path(r'\\wsl.localhost\Ubuntu\home\djc\.cache\arkforge\signer-ohos-ndk\libsigntool.so')
data = source.read_bytes()
if data[:4] != b'\x7fELF' or int.from_bytes(data[18:20], 'little') != 183:
    raise RuntimeError('Signer must be an AArch64 ELF')
target = project / 'entry/libs/arm64-v8a/libsigntool.so'
target.parent.mkdir(parents=True, exist_ok=True)
shutil.copy2(source, target)
fixture = project / 'harmony_demo/build/default/outputs/default/harmony_demo-default-signed.hap'
raw = project / 'entry/src/main/resources/rawfile/backend'
raw.mkdir(parents=True, exist_ok=True)
shutil.copy2(fixture, raw / 'verify-demo.hap')
shutil.copy2(fixture.with_name('harmony_demo-default-unsigned.hap'), raw / 'unsigned-demo.hap')
(raw / 'signer-build.json').write_text(json.dumps({
    'architecture': 'AArch64', 'sha256': hashlib.sha256(data).hexdigest(),
    'purpose': 'Native signing and verification; contains no private key.'
}, indent=2), encoding='utf-8')
print('Staged native signer and signed verification fixture; no credentials copied.')
