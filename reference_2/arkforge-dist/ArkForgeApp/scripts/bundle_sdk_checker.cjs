// Capture the unmodified installed SDK modules and their declaration filesystem.
const fs = require('fs'), path = require('path'), Module = require('module'), vm = require('vm');
const sdk = path.resolve(process.env.ARKFORGE_TRANSFORM_SDK || 'D:/tools/HarmonyOS/26.0.0.851/command-line-tools/sdk/default/openharmony/ets/build-tools/ets-loader');
const hms = path.resolve(sdk,'../../../../hms/ets');
const runtime = path.join(__dirname,'sdk_check_runtime.cjs');
const virtualSdk = '/sdk/ets/build-tools/ets-loader';
const mounts = [[sdk,virtualSdk],[hms,'/hms'],[path.resolve(sdk,'../../api'),'/sdk/ets/api'],[path.resolve(sdk,'../../kits'),'/sdk/ets/kits'],[path.resolve(sdk,'../../arkts'),'/sdk/ets/arkts']];
const read = fs.readFileSync, load = Module._load, dirs = fs.readdirSync;
const assets = {}, directories = {}, mapping = {};
function id(filename) {
  const full = path.resolve(filename);
  if (full === runtime) return '/checker.cjs';
  for (const [base,mount] of mounts) if(full === base || full.startsWith(base+path.sep)) return mount + '/' + path.relative(base,full).replaceAll('\\','/');
  return full.replaceAll('\\','/');
}
fs.readFileSync = function(name,...args) {
  const result = read.call(fs,name,...args);
  if(typeof name === 'string' && mounts.some(([base]) => path.resolve(name).startsWith(base+path.sep))) assets[id(name)] = result.toString('utf8');
  return result;
};
fs.readdirSync = function(name,...args) {
  const result = dirs.call(fs,name,...args);
  if(mounts.some(([base]) => path.resolve(name).startsWith(base))) directories[id(name).replace(/\/$/,'')] = result.map(e => typeof e === 'string' ? e : e.name);
  return result;
};
Module._load = function(request,parent) {
  const resolved = Module._resolveFilename(request,parent);
  const key = path.isAbsolute(request) ? id(request) : request;
  (mapping[parent ? id(parent.filename) : ''] ||= {})[key] = path.isAbsolute(resolved) ? id(resolved) : resolved;
  if(request === 'log4js') return {getLogger:()=>({trace(){},debug(){},info(){},warn(){},error(){}}),configure(){}};
  if(request.startsWith('webpack/') || request.startsWith('webpack-sources/')) return class { constructor(){throw new Error('Unexpected desktop bundler operation');} };
  return load.apply(this,arguments);
};
globalThis.__arkforgeSdk = sdk;
process.env.externalApiPaths = hms;
const fixtureRoot = path.resolve(__dirname,'../.checker-bundle-probe');
const filename = fixtureRoot+'/entry/src/main/ets/Probe.ets';
fs.mkdirSync(path.dirname(filename),{recursive:true});
const text = '@Entry\n@Component\nstruct Probe {\n @State count:number="wrong";\n build(){Text(this.count.toString())}\n}';
fs.writeFileSync(filename,text);
const bootstrap = require(runtime).check({projectRoot:fixtureRoot,moduleName:'entry',files:{[filename]:text}});
if(!bootstrap.diagnostics.some(d=>d.code===2322)) throw new Error(JSON.stringify(bootstrap));
Module._load = load; fs.readFileSync = read; fs.readdirSync = dirs;
// Read every SDK declaration, preserving contents and notices verbatim.
function capture(directory) {
  if(!fs.existsSync(directory)) return;
  directories[id(directory).replace(/\/$/,'')] = dirs.call(fs,directory);
  for(const entry of dirs.call(fs,directory,{withFileTypes:true})) {
    // Desktop build-tool dependency typings are not target SDK declarations.
    if(directory === path.join(sdk,'node_modules') && entry.name !== 'typescript') {
      const pkg = path.join(directory,entry.name,'package.json');
      if(fs.existsSync(pkg)) assets[id(pkg)] = read.call(fs,pkg,'utf8');
      continue;
    }
    if(directory === sdk && entry.name === 'bin') continue;
    const name = path.join(directory,entry.name);
    if(entry.isDirectory()) capture(name);
    else if(/\.d\.(ts|ets)$/.test(name) || entry.name === 'package.json' ||
      (path.basename(directory)==='device-define' && entry.name.endsWith('.json') && fs.statSync(name).size<1024*1024)) assets[id(name)] = read.call(fs,name,'utf8');
  }
}
for(const [base] of mounts) capture(base);
const modules = {};
for(const filename of Object.keys(require.cache)) {
  if(filename !== runtime && !mounts.some(([base])=>filename.startsWith(base+path.sep))) continue;
  modules[id(filename)] = filename.endsWith('.json') ? 'module.exports='+read.call(fs,filename,'utf8') : read.call(fs,filename,'utf8');
}
for(const key of Object.keys(modules)) if(key.endsWith('.js')) delete assets[key];
const prelude = read.call(fs,path.join(__dirname,'ui_transform_shims.js'),'utf8')+'\n'+read.call(fs,path.join(__dirname,'sdk_check_host.js'),'utf8');
const bundle = prelude+'\nvar __moduleBodies={'+Object.entries(modules).map(([key,body])=>JSON.stringify(key)+':function(require,module,exports,__filename,__dirname){\n'+body+'\n}').join(',\n')+'};\nvar __moduleMap='+JSON.stringify(mapping)+';\nvar __sdkFiles='+JSON.stringify(assets)+';\nvar __sdkDirectories='+JSON.stringify(directories)+';\n'+
 'var __sdkContentHashes='+JSON.stringify(Object.fromEntries(Object.entries(assets).map(([name,text])=>[name,require('crypto').createHash('sha256').update(text).digest('hex')])))+';__initializeSdkHashCache(__sdkFiles,__sdkContentHashes);\n'+
 'globalThis.__arkforgeSdk='+JSON.stringify(virtualSdk)+';process.env.externalApiPaths="/hms";JSON.stringify(__load("/checker.cjs").check(__arkforgeOptions));';
const output = path.join(__dirname,'../entry/src/main/resources/rawfile/compiler/sdk-check.bundle.js');
fs.writeFileSync(output,bundle);
fs.writeFileSync(path.join(path.dirname(output),'sdk-check.json'),JSON.stringify({sdkVersion:'26.0.0.851',sha256:require('crypto').createHash('sha256').update(bundle).digest('hex')}));
console.log(JSON.stringify({modules:Object.keys(modules).length,assets:Object.keys(assets).length,bytes:Buffer.byteLength(bundle)}));
fs.writeFileSync(path.join(__dirname,'../sdk-check-assets.json'), JSON.stringify(Object.entries(assets).map(([name,text])=>({name,bytes:Buffer.byteLength(text)})).sort((a,b)=>b.bytes-a.bytes),null,2));
const options = {moduleName:'entry',files:{'/project/entry/src/main/ets/Probe.ets':text}};
const checked = JSON.parse(vm.runInNewContext(bundle,{__arkforgeOptions:options,console,setTimeout,clearTimeout},{timeout:120000}));
if(!checked.diagnostics.some(d=>d.code===2322)) throw new Error(JSON.stringify(checked));
console.log('Isolated official SDK checker passed',JSON.stringify(checked));
