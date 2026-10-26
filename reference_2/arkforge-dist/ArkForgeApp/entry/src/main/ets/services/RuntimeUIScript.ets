export interface UIAction { id?: string; text?: string; optional?: boolean; }

/** Bounded orchestration of system tools. Labels are data, never shell/SQL code. */
export class RuntimeUIScript {
  private static shell(value: string): string { return "'" + value.replace(/'/g,"'\\''") + "'"; }
  private static sql(value: string): string { return "'" + value.replace(/'/g,"''") + "'"; }
  static render(bundle: string, ability: string, editor: string, actions: UIAction[], expected: string,
    requestId: string, prefix: string): string {
    const lines: string[] = [
      '#!/system/bin/sh', 'umask 077',
      'prefix=' + RuntimeUIScript.shell(prefix),
      'editor=' + RuntimeUIScript.shell(editor),
      'result="$prefix.result"', 'layout="$prefix.layout.json"',
      'printf "%s\\n" ' + RuntimeUIScript.shell('requestId:' + requestId) + ' > "$result"',
      'finish() {',
      '  trap - EXIT HUP INT TERM',
      '  if timeout 20 aa start -b "$editor" -a EntryAbility; then',
      '    echo restored:1 >> "$result"',
      '  else echo restored:0 >> "$result"; fi',
      '  echo complete:1 >> "$result"',
      '}', 'trap finish EXIT', 'trap "exit 1" HUP INT TERM',
      'fail() { printf "error:%s\\n" "$1" >> "$result"; exit 1; }',
      'started=$(date +%s)',
      'inspect() {',
      '  [ $(($(date +%s)-started)) -lt 150 ] || fail time-window',
      '  timeout 20 uitest dumpLayout -b ' + RuntimeUIScript.shell(bundle) + ' -p "$layout" > "$prefix.log" 2>&1 || fail layout-command',
      '  [ -s "$layout" ] && [ "$(wc -c < "$layout")" -le 16777216 ] || fail layout-size',
      '}',
      'bounds() {',
      '  {',
      '    printf "WITH nodes AS (SELECT value FROM json_tree(\'"',
      '    sed "s/\'/\'\'/g" "$layout"',
      '    printf "\') WHERE key=\'attributes\') SELECT DISTINCT json_extract(value,\'$.bounds\') FROM nodes WHERE coalesce(json_extract(value,\'$.visible\'),\'true\')!=\'false\' AND coalesce(json_extract(value,\'$.enabled\'),\'true\')!=\'false\' AND %s;\\n" "$1"',
      '  } | timeout 10 sqlite3 :memory: 2>> "$prefix.log" | grep -E "^\\[[0-9]+,[0-9]+\\]\\[[0-9]+,[0-9]+\\]$"',
      '}',
      'tap() {',
      '  inspect',
      '  matches=$(bounds "$2")',
      '  if [ -z "$matches" ] && [ "$3" = 1 ]; then echo "step:$1:not-present" >> "$result"; return; fi',
      '  [ "$(printf "%s\\n" "$matches" | grep -c .)" = 1 ] || fail "selector-$1"',
      '  coords=$(printf "%s\\n" "$matches" | sed -n "s/^\\[\\([0-9]*\\),\\([0-9]*\\)\\]\\[\\([0-9]*\\),\\([0-9]*\\)\\]$/\\1 \\2 \\3 \\4/p")',
      '  step="$1"', '  set -- $coords',
      '  [ "$#" = 4 ] && [ "$3" -gt "$1" ] && [ "$4" -gt "$2" ] && [ "$3" -lt 100000 ] && [ "$4" -lt 100000 ] || fail bounds',
      '  timeout 20 uitest uiInput click $((($1+$3)/2)) $((($2+$4)/2)) > "$prefix.log" 2>&1 || fail click-command',
      '  echo "step:$step:clicked" >> "$result"', '  sleep 0.35', '}',
      'timeout 20 aa start -b ' + RuntimeUIScript.shell(bundle) + ' -a ' + RuntimeUIScript.shell(ability) + ' > "$prefix.log" 2>&1 || fail launch-command',
      'sleep 0.6'
    ];
    for (let i = 0; i < actions.length; i++) {
      const action = actions[i];
      const selector = 'json_extract(value,' + RuntimeUIScript.sql(action.id ? '$.id' : '$.text') + ')=' + RuntimeUIScript.sql(action.id || action.text || '');
      lines.push('tap ' + i + ' ' + RuntimeUIScript.shell(selector) + ' ' + (action.optional ? '1' : '0'));
    }
    lines.push('sleep 0.6', 'inspect');
    if (expected) {
      const selector = 'json_extract(value,\'$.id\')=' + RuntimeUIScript.sql(expected);
      lines.push('[ -n "$(bounds ' + RuntimeUIScript.shell(selector) + ')" ] || fail expected-control');
    }
    lines.push('echo expected:1 >> "$result"', 'exit 0', '');
    return lines.join('\n');
  }
}
