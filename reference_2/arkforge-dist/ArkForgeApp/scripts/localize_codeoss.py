"""Map official Chinese translations to this exact Code-OSS build's NLS indices."""
import json
import re
from ui_branding import brand_text
from pathlib import Path

app = Path(__file__).resolve().parent.parent
source = app.parents[2] / '_vendor/vscode/out-arkforge-web'
local = app / 'workbench/localization'
keys = json.loads((source / 'nls.keys.json').read_text(encoding='utf8'))
english = json.loads((source / 'nls.messages.json').read_text(encoding='utf8'))
translations = json.loads((local / 'main.i18n.json').read_text(encoding='utf8'))['contents']
by_key = {}
for entries in translations.values():
    for key, value in entries.items():
        if isinstance(value,str): by_key.setdefault(key,set()).add(value)
messages = []
matched = 0
supplement = {'Default permissions':'默认权限','Allow all':'允许全部','Layout: {0}':'布局：{0}','Agent':'智能体'}
for module, names in keys:
    for key in names:
        original = english[len(messages)]
        translated = translations.get(module, {}).get(key)
        if original in supplement: translated = supplement[original]
        if translated is None and len(by_key.get(key,set())) == 1:
            translated = next(iter(by_key[key]))
        if isinstance(translated, str) and sorted(re.findall(r'\{\d+\}', translated)) == sorted(re.findall(r'\{\d+\}', original)):
            messages.append(translated)
            matched += 1
        else:
            messages.append(original)
messages = [brand_text(message) for message in messages]
assert len(messages) == len(english)
assert matched > len(messages) * .75
(local / 'nls.messages.zh-cn.js').write_text('/* Microsoft vscode-loc translations, MIT. See LICENSE.txt. */\n'
    'globalThis._VSCODE_NLS_LANGUAGE="zh-cn";\nglobalThis._VSCODE_NLS_MESSAGES='
    + json.dumps(messages, ensure_ascii=False) + ';\n', encoding='utf8')
(local / 'coverage.json').write_text(json.dumps({'total':len(messages),'matched':matched,'fallback':len(messages)-matched},indent=2),encoding='utf8')
print(f'Chinese NLS: {matched}/{len(messages)} keys matched; indexed against current build.')
