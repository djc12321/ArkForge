// Build a self-contained CommonJS module graph and virtual SDK filesystem.
const fs = require('fs');
const path = require('path');
const Module = require('module');
const sdk = process.env.ARKFORGE_TRANSFORM_SDK || 'D:/tools/HarmonyOS/26.0.0.851/command-line-tools/sdk/default/openharmony/ets/build-tools/ets-loader';
const sdkAbs = path.resolve(sdk);
const hms = path.resolve(sdk, '../../../../hms/ets');
globalThis.__arkforgeSdk = sdk;
process.env.externalApiPaths = hms;
const runtime = path.resolve(__dirname, 'ui_transform_runtime.cjs');
const originalLoad = Module._load, originalRead = fs.readFileSync, originalReaddir = fs.readdirSync;
const mapping = {}, assets = {}, directories = {}, builtins = new Set();
function id(name) {
  const absolute = path.resolve(name);
  if (absolute === runtime) return '/runtime.cjs';
  if (absolute.startsWith(sdkAbs + path.sep)) return '/sdk/' + path.relative(sdkAbs, absolute).replaceAll('\\','/');
  if (absolute.startsWith(hms + path.sep)) return '/hms/' + path.relative(hms, absolute).replaceAll('\\','/');
  return absolute.replaceAll('\\','/');
}
fs.readFileSync = function(name, options) {
  const result = originalRead.call(fs, name, options);
  if (typeof name === 'string' && (path.resolve(name).startsWith(sdkAbs + path.sep) || path.resolve(name).startsWith(hms + path.sep))) assets[id(name)] = result.toString('utf8');
  return result;
};
fs.readdirSync = function(name, options) {
  const result = originalReaddir.call(fs, name, options);
  if (path.resolve(name).startsWith(sdkAbs + path.sep) || path.resolve(name).startsWith(hms + path.sep)) directories[id(name)] = result.map(item => typeof item === 'string' ? item : item.name);
  return result;
};
Module._load = function(request, parent, isMain) {
  let resolved;
  try { resolved = Module._resolveFilename(request, parent); } catch(error) { throw error; }
  const parentId = parent ? id(parent.filename) : '';
  const builtin = !path.isAbsolute(resolved);
  if (builtin) builtins.add(resolved);
  if (!mapping[parentId]) mapping[parentId] = {};
  mapping[parentId][path.isAbsolute(request) ? id(request) : request] = builtin ? resolved : id(resolved);
  if (request === 'log4js') return {getLogger:()=>console, configure:()=>{}};
  // The SDK's diagnostics module imports desktop bundler plugins eagerly.
  // They are not used by transpileModule; fail explicitly if invoked.
  if (request.startsWith('webpack/') || request.startsWith('webpack-sources/')) return class DesktopBundlerUnavailable {
    constructor(){throw new Error('Desktop bundler is unavailable in device transformer');}
  };
  return originalLoad.apply(this, arguments);
};
const api = require(runtime);
const source = originalRead.call(fs, path.join(__dirname, '../harmony_demo/src/main/ets/pages/Index.ets'), 'utf8');
const result = api.transform(source);
if (!result.ok) throw new Error(JSON.stringify(result.errors));
const abilitySource = originalRead.call(fs,path.join(__dirname,'../harmony_demo/src/main/ets/harmonydemoability/HarmonyDemoAbility.ets'),'utf8');
const abilityResult = api.transform(abilitySource,'/project/harmony_demo/src/main/ets/harmonydemoability/HarmonyDemoAbility.ets');
if (!abilityResult.ok) throw new Error(JSON.stringify(abilityResult.errors));
Module._load = originalLoad; fs.readFileSync = originalRead; fs.readdirSync = originalReaddir;
for (const [parent, deps] of Object.entries(mapping)) {
  if (parent.startsWith('/sdk/lib/') && Object.values(deps).some(value => value.includes('/node_modules/webpack/'))) console.log('Desktop build dependency:', parent, JSON.stringify(deps));
}
const modules = {};
for (const filename of Object.keys(require.cache)) {
  if (filename !== runtime && !filename.startsWith(sdkAbs + path.sep) && !filename.startsWith(hms + path.sep)) continue;
  if (filename === path.join(sdkAbs,'main.js')) continue; // runtime installs the project-specific object
  modules[id(filename)] = filename.endsWith('.json') ? 'module.exports=' + originalRead.call(fs,filename,'utf8') : originalRead.call(fs,filename,'utf8');
}
const prelude = originalRead.call(fs,path.join(__dirname,'ui_transform_shims.js'),'utf8');
for (const filename of originalReaddir.call(fs,path.join(sdk,'kit_configs'))) {
  if (filename.endsWith('.json')) assets['/sdk/kit_configs/'+filename] = originalRead.call(fs,path.join(sdk,'kit_configs',filename),'utf8');
}
for (const filename of originalReaddir.call(fs,path.join(hms,'build-tools/ets-loader/kit_configs'))) {
  if (filename.endsWith('.json')) assets['/sdk/kit_configs/'+filename] = originalRead.call(fs,path.join(hms,'build-tools/ets-loader/kit_configs',filename),'utf8');
}
for (const key of Object.keys(assets)) {
  if (modules[key] && key.endsWith('.js')) delete assets[key];
}
const bundle = prelude + '\nvar __moduleBodies={' + Object.entries(modules).map(([key,body])=>JSON.stringify(key)+':function(require,module,exports,__filename,__dirname){\n'+body+'\n}').join(',\n') + '};\nvar __moduleMap='+JSON.stringify(mapping)+';\nvar __sdkFiles='+JSON.stringify(assets)+';\nvar __sdkDirectories='+JSON.stringify(directories)+';\n' +
  'globalThis.__arkforgeSdk="/sdk";process.env.externalApiPaths="/hms";\n' +
  '(function(){var api=__load("/runtime.cjs"),options=typeof __arkforgeOptions === "undefined"?{}:__arkforgeOptions;' +
  'if(typeof __arkforgeBatchFileNames !== "undefined"){var outputs=[];for(var i=0;i<__arkforgeBatchFileNames.length;i++){' +
  'var name=__arkforgeBatchFileNames[i];console.log("ArkUI transform ["+(i+1)+"/"+__arkforgeBatchFileNames.length+"] "+name);' +
  'var result=api.transform(options.files[name],name,options);if(!result.ok)return JSON.stringify({ok:false,error:name,errors:result.errors});' +
  'outputs.push({fileName:name,output:result.output,sourceMap:result.sourceMap,generatedLineMap:result.generatedLineMap});}return JSON.stringify({ok:true,outputs:outputs});}' +
  'return JSON.stringify(api.transform(__arkforgeSource,__arkforgeFileName,options));})()';
const output = path.join(__dirname,'../entry/src/main/resources/rawfile/compiler/ui-transform.bundle.js');
fs.writeFileSync(output,bundle);
console.log(JSON.stringify({modules:Object.keys(modules).length,assets:Object.keys(assets).length,bytes:bundle.length,builtins:[...builtins]}));
// The bundle check has no Node require/fs/process available to the compiler.
const vm = require('vm');
const check = vm.runInNewContext(bundle,{__arkforgeSource:source,__arkforgeFileName:'/project/harmony_demo/src/main/ets/pages/Index.ets',console,setTimeout,clearTimeout},{timeout:120000});
fs.writeFileSync(path.join(__dirname,'../ui-transform-isolated-result.json'),check);
if (!JSON.parse(check).ok) throw new Error(check);
console.log('Isolated SDK transform passed');
