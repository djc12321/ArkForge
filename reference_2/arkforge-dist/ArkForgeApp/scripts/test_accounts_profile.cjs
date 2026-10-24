'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const ts=require('C:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/ets/build-tools/ets-loader/node_modules/typescript');
const source=fs.readFileSync(path.resolve('../../../_vendor/vscode/src/vs/workbench/browser/parts/globalCompositeBar.ts'),'utf8');
const start=source.indexOf('// Native credentials stay'),end=source.indexOf('export class GlobalCompositeBar',start);
assert(start>=0&&end>start);let handler,events=0;
const sandbox={CommandsRegistry:{registerCommand:(id,fn)=>{assert.equal(id,'_arkforge.setAccountProfile');handler=fn;}},Emitter:class {fire(){events++;}}};
vm.runInNewContext(ts.transpileModule(source.slice(start,end),{compilerOptions:{target:ts.ScriptTarget.ES2020}}).outputText,sandbox);
handler({}, {login:'Tester',avatar:'https://avatars.githubusercontent.com/u/42?v=4'});assert.equal(events,1);
for(const avatar of ['http://avatars.githubusercontent.com/u/42','https://evil.invalid/u/42','https://avatars.githubusercontent.com.evil.invalid/u/42','javascript:alert(1)']) {
  assert.throws(()=>handler({}, {login:'Tester',avatar}),/Invalid GitHub avatar/);
}
assert.throws(()=>handler({}, {login:'<script>'}),/Invalid GitHub account/);assert.equal(events,1);
handler({},undefined);assert.equal(events,2,'logout must clear the profile and notify the native avatar view');
console.log('Native accounts profile: GitHub CDN allowlist, username validation, profile update and logout notification passed.');
