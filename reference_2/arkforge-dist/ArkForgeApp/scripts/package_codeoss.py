"""Package the upstream Code-OSS web workbench, web dependencies and device extension."""
import hashlib
import json
import subprocess
import zipfile
from pathlib import Path

app = Path(__file__).resolve().parent.parent
upstream = app.parents[2] / '_vendor' / 'vscode'
output = upstream / 'out-arkforge-web'
if not (output / 'vs/workbench/workbench.web.main.internal.js').is_file():
    raise RuntimeError('Build official Code-OSS web target before packaging')
raw = app / 'entry/src/main/resources/rawfile'
raw.mkdir(parents=True, exist_ok=True)
archive = raw / 'codeoss.zip'
subprocess.run(['node', str(app / 'scripts/bundle_codeoss_extension.cjs')], check=True)
subprocess.run(['python', str(app / 'scripts/localize_codeoss.py')], check=True)
extensions = ['extensions/arkforge-device']
files = {}

def add_tree(root, prefix):
    for path in root.rglob('*'):
        if not path.is_file() or path.is_symlink():
            continue
        relative = path.relative_to(root)
        if path.suffix in ('.map', '.ts', '.mts') or any(part in ('test', 'tests', '.bin', '.git') for part in relative.parts):
            continue
        files[prefix + '/' + relative.as_posix()] = path

add_tree(output, 'out')
files['out/nls.messages.js'] = app / 'workbench/localization/nls.messages.zh-cn.js'
add_tree(app / 'workbench/localization', 'localization')
add_tree(upstream / 'remote/web/node_modules', 'node_modules')
for directory in sorted((upstream / 'extensions').iterdir()):
    manifest = directory / 'package.json'
    if not manifest.is_file():
        continue
    data = json.loads(manifest.read_text(encoding='utf-8'))
    # Declarative upstream languages/themes work without desktop native binaries.
    if data.get('main') or data.get('browser'):
        continue
    extensions.append('extensions/' + directory.name)
    add_tree(directory, 'extensions/' + directory.name)
add_tree(app / 'workbench/extension', 'extensions/arkforge-device')
files['extensions/arkforge-device/extension.js'] = app / '.workbench-build/extension.js'
files['index.html'] = app / 'workbench/index.html'
files['phone.css'] = app / 'workbench/phone.css'
files['phoneLayout.js'] = app / 'workbench/phoneLayout.js'
files['LICENSE.txt'] = upstream / 'LICENSE.txt'
files['arkforge-patches/accounts-context.patch'] = app / 'workbench/patches/accounts-context.patch'
files['arkforge-patches/terminal-default-profile.patch'] = app / 'workbench/patches/terminal-default-profile.patch'
files['arkforge-patches/welcome-guide.patch'] = app / 'workbench/patches/welcome-guide.patch'
files['arkforge-patches/chat-primary-action.patch'] = app / 'workbench/patches/chat-primary-action.patch'
for name in ('ThirdPartyNotices.txt', 'ThirdPartyNotices.text'):
    if (upstream / name).exists():
        files[name] = upstream / name
with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as bundle:
    for name, path in sorted(files.items()):
        bundle.write(path, name)
    bundle.writestr('extensions.json', json.dumps(extensions))
commit = subprocess.check_output(['git', '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip()
metadata = {
    'upstream': 'https://github.com/microsoft/vscode', 'commit': commit,
    'version': json.loads((upstream / 'package.json').read_text(encoding='utf-8'))['version'],
    'sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
    'files': len(files) + 1, 'builtinExtensions': len(extensions), 'bytes': archive.stat().st_size,
    'license': 'MIT', 'target': 'web', 'hostRuntimeRequired': False,
    'sourcePatches': ['terminal-default-profile.patch: create the sole contributed terminal profile directly from New Terminal', 'welcome-guide.patch: expose the native Help menu for the first-row ArkForge guide', 'accounts-context.patch: expose MenuId.AccountsContext and supply a validated public GitHub profile to the existing avatar renderer; no layout/CSS changes',
                      'chat-primary-action.patch: open ArkForge Agent from the chat bubble primary action; avoid duplicate split-button touch activation, enable workspace touch search and outside-pointer dismissal, and preserve layout'],
}
(raw / 'codeoss-manifest.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
print(json.dumps(metadata))
