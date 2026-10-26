"""Exercise generated POSIX orchestration with real SQLite JSON queries."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

cases = json.load(sys.stdin)
with tempfile.TemporaryDirectory(prefix='arkforge-ui-test-') as temporary:
    root = Path(temporary)
    binaries = root / 'bin'
    binaries.mkdir()
    programs = {
        'sleep': '#!/bin/sh\nexit 0\n',
        'sqlite3': '''#!/usr/bin/python3
import sqlite3,sys
try:
 for row in sqlite3.connect(':memory:').execute(sys.stdin.read()):
  print('' if row[0] is None else row[0])
except Exception as e:
 print(e,file=sys.stderr);sys.exit(1)
''',
        'aa': '''#!/usr/bin/python3
import json,os,sys
p=os.environ['ARKFORGE_UI_TEST_STATE'];s=json.load(open(p))
if 'com.editor.app' in sys.argv:s['restored']=True
elif s.get('launchFailure'):sys.exit(1)
json.dump(s,open(p,'w'));print('start ability successfully.')
''',
        'uitest': '''#!/usr/bin/python3
import json,os,sys
p=os.environ['ARKFORGE_UI_TEST_STATE'];s=json.load(open(p))
if sys.argv[1]=='dumpLayout':
 controls=s['screens'][min(s.get('clicks',0),len(s['screens'])-1)]
 text='invalid' if s.get('invalidJSON') else json.dumps({'attributes':{},'children':[{'attributes':c} for c in controls]})
 open(sys.argv[sys.argv.index('-p')+1],'w').write(text)
elif sys.argv[1]=='uiInput':
 if s.get('clickFailure'):sys.exit(1)
 s['clicks']=s.get('clicks',0)+1;json.dump(s,open(p,'w'))
else:sys.exit(1)
'''
    }
    for name, source in programs.items():
        path = binaries / name
        path.write_text(source, encoding='utf-8')
        path.chmod(0o700)
    for case in cases:
        prefix = root / case['name']
        state = prefix.with_suffix('.state')
        state.write_text(json.dumps(case), encoding='utf-8')
        script = prefix.with_suffix('.sh')
        script.write_text(case['script'].replace('PREFIX_PLACEHOLDER', str(prefix)), encoding='utf-8')
        env = dict(os.environ, PATH=str(binaries)+':'+os.environ['PATH'], ARKFORGE_UI_TEST_STATE=str(state))
        result = subprocess.run(['sh', str(script)], env=env, capture_output=True, text=True, timeout=15)
        records = prefix.with_suffix('.result').read_text()
        assert ('expected:1' in records) == case['passed'], (case['name'],records,result.stderr)
        assert 'requestId:'+case['name'] in records and 'complete:1' in records, records
        assert 'restored:1' in records and json.loads(state.read_text()).get('restored'), records
        assert not Path('/tmp/arkforge-injection-test').exists(), 'selector escaped into a shell command'
        if case['name']=='duplicates-optional':
            assert 'step:0:not-present' in records and 'step:1:clicked' in records
    print(f"PASS: {len(cases)} real SQLite/POSIX cases: exact selectors, quote/injection safety, duplicate bounds, optional/hidden nodes, launch/click/layout failures, expected controls and editor restoration.")
