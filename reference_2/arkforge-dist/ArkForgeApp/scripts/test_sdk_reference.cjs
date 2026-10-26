const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const bundle=fs.readFileSync('entry/src/main/resources/rawfile/compiler/sdk-check.bundle.js','utf8');
function lookup(options){return JSON.parse(vm.runInNewContext(bundle,{__arkforgeOptions:{operation:'lookupAPI',...options},console,setTimeout,clearTimeout},{timeout:120000}));}
const result=lookup({query:'WithTheme'});assert(result.matched_files>0);assert.match(JSON.stringify(result.results),/colorMode/);assert.match(JSON.stringify(result.results),/@since/);assert(result.results.every(r=>r.sdk_path.startsWith('/sdk/ets/')||r.sdk_path.startsWith('/hms/')));
const file=result.results.find(r=>r.sdk_path.includes('with_theme'))?.sdk_path||result.results[0].sdk_path;
const page=lookup({sdk_path:file,start_line:1,end_line:200});assert.equal(page.results[0].sdk_path,file);assert.match(page.results[0].content,/Copyright/);
assert.equal(lookup({sdk_path:'/project/private-key'}).results.length,0);
console.log('PASS: real bundled official SDK lookup returns declarations, parameter/version documentation and bounded source ranges; project paths are excluded.');
