"""Verify pulled device artifacts against the original imported project."""
import hashlib
import json
import sys
from pathlib import Path
from zipfile import ZipFile

result_path, abc_path, signed_path, source_zip, evidence = map(Path, sys.argv[1:6])
overrides = json.loads(Path(sys.argv[6]).read_text(encoding='utf8')) if len(sys.argv) > 6 else {}
result = json.loads(result_path.read_text(encoding='utf8'))
abc = abc_path.read_bytes()
assert abc.startswith(b'PANDA'), 'Not an Ark bytecode file'
assert hashlib.sha256(abc).hexdigest() == result['abcSha256'].lower()
assert hashlib.sha256(signed_path.read_bytes()).hexdigest() == result['signedHapSha256'].lower()
with ZipFile(signed_path) as signed:
    assert signed.read('ets/modules.abc') == abc, 'Installed candidate contains another ABC'
    module = json.loads(signed.read('module.json'))
    assert module['app']['bundleName'] == result['bundleName']
    assert module['module']['name'] == result['moduleName']
    assert 'libs/arm64-v8a/libentry.so' in signed.namelist()
    assert signed.getinfo('resources.index').compress_type == 0, 'Resource index must be stored, as in SDK HAPs'
with ZipFile(source_zip) as source:
    expected = {name for name in source.namelist()
        if name.startswith('entry/src/main/ets/') and name.endswith(('.ets', '.ts'))}
    actual = {item['path'] for item in result['sources']}
    assert actual == expected, 'Incomplete source compilation'
    for item in result['sources']:
        data = Path(overrides[item['path']]).read_bytes() if item['path'] in overrides else source.read(item['path'])
        assert hashlib.sha256(data).hexdigest() == item['sha256'].lower(), item['path']
report = dict(verified=True, buildId=result['id'], bundleName=result['bundleName'],
    moduleName=result['moduleName'], sourceFiles=len(actual), abcBytes=len(abc),
    abcVersion=list(abc[12:16]), abcSha256=result['abcSha256'], signedHapSha256=result['signedHapSha256'],
    allImportedSourceHashesMatch=True, reusedAbc=result.get('reusedAbc', False),
    sourceOverrides=list(overrides), resourceIndexStored=True,
    signedHapContainsExactDeviceAbc=True,
    mode='offline-incremental-resources-native-cache')
evidence.write_text(json.dumps(report, indent=2), encoding='utf8')
print(json.dumps(report))
