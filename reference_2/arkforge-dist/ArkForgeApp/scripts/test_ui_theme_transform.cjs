const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source='@Component\nexport struct ThemeProbe {build(){WithTheme({colorMode:ThemeColorMode.LIGHT}){Text("theme")}}}';
const bundle=fs.readFileSync('entry/src/main/resources/rawfile/compiler/ui-transform.bundle.js','utf8');
const result=JSON.parse(vm.runInNewContext(bundle,{__arkforgeSource:source,__arkforgeFileName:'/project/entry/src/main/ets/ThemeProbe.ets',__arkforgeOptions:{moduleName:'entry',normalized:true},console,setTimeout,clearTimeout},{timeout:120000}));
assert(result.ok,JSON.stringify(result.errors));assert.match(result.output,/observeComponentCreation2/);assert.match(result.output,/WithTheme\.create/);assert.match(result.output,/}, WithTheme\)/);assert(!result.output.includes('this.observeComponentCreation('),'theme scopes require the official optimized component lifecycle');
const metadata=JSON.parse(fs.readFileSync('entry/src/main/resources/rawfile/compiler/ui-transform.json','utf8'));assert.equal(metadata.partialUpdateConfig.optimizeComponent,true);assert.equal(require('node:crypto').createHash('sha256').update(bundle).digest('hex'),metadata.sha256);
console.log('PASS: isolated official SDK emits optimized WithTheme creation with component metadata, matching the DevEco theme lifecycle contract.');
