"""Check original SDK diagnostics; this host probe is not device validation."""
import json
import subprocess
import sys
from pathlib import Path

node = r'C:\Program Files\Huawei\DevEco Studio\tools\node\node.exe'
script = Path(__file__).with_name('probe_sdk_checker.cjs')
output = Path(sys.argv[1]).resolve()
output.mkdir(parents=True, exist_ok=True)
template = '@Entry\n@Component\nstruct Probe {\n  @State count: TYPE = VALUE;\n  build() { BODY }\n}\n'
cases = [
    ('valid', 'number', '1', 'Text(this.count.toString())', None),
    ('type_mismatch', 'number', '"wrong"', 'Text(this.count.toString())', 2322),
    ('undefined_name', 'number', '1', 'Text(missingValue)', 2304),
    ('arkts_rule', 'any', '1', 'Text(this.count.toString())', 'linter'),
]
reports = []
for name, kind, value, body, expected in cases:
    source = template.replace('TYPE', kind).replace('VALUE', value).replace('BODY', body)
    result = subprocess.run([node, str(script), str(output / name), source],
                            capture_output=True, encoding='utf8', errors='replace', check=True)
    raw, lint_raw = result.stdout.split('ARKTS_LINTER', 1)
    diagnostics, lint = json.loads(raw), json.loads(lint_raw)
    errors = [d for d in diagnostics if d['category'] == 1]
    if expected is None:
        assert not errors and not lint, (name, errors, lint)
    elif expected == 'linter':
        assert any(d['code'] == 8 and d['line'] == 4 for d in lint), lint
    else:
        assert any(d['code'] == expected and d['line'] == (4 if expected == 2322 else 5)
                   for d in errors), errors
    reports.append(dict(case=name, diagnostics=diagnostics, lint=lint, passed=True))
report = dict(scope='Host-side direct official SDK checker; not integrated on device',
              sdk='26.0.0.851', cases=reports, allPassed=True)
(output / 'sdk-diagnostics-check.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
print(json.dumps(dict(allPassed=True, cases=[r['case'] for r in reports])))
