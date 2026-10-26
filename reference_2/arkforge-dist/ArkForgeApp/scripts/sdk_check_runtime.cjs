// The SDK owns parsing, module/type checking, API checks and ArkTS rules.
const fs = require('fs'), path = require('path');
const sdk = globalThis.__arkforgeSdk;
exports.check = function(options) {
  if(options.operation==='lookupAPI')return lookupAPI(options,typeof __sdkFiles==='undefined'?{}:__sdkFiles);
  const root = options.projectRoot || '/project';
  const files = options.files || {};
  const json5 = require(path.join(sdk, 'node_modules/json5'));
  if (options.operation === 'projectPlan') {
    const profile = json5.parse(files[root+'/build-profile.json5'] || '{}');
    const products = profile.app && profile.app.products || [];
    const product = products.find(p=>p.name===(options.product || 'default')) || products[0] || {};
    const app = json5.parse(files[root+'/AppScope/app.json5'] || '{}').app;
    const entryModule = (profile.modules || []).find(m=>{
      const config=files[root+'/'+String(m.srcPath || m.name).replace(/^\.\//,'')+'/src/main/module.json5'];
      return config && json5.parse(config).module.type === 'entry';
    });
    const moduleName = entryModule ? String(entryModule.srcPath || entryModule.name).replace(/^\.\//,'') : options.moduleName || 'entry';
    const module = json5.parse(files[root+'/'+moduleName+'/src/main/module.json5'] || '{}').module;
    if (!app || !module) throw new Error('Missing HarmonyOS app/module configuration');
    function apiVersion(version) {
      if(typeof version==='number') return version;
      const match=/^(\d+)\.(\d+)\.(\d+)(?:\((\d+)\))?$/.exec(String(version));
      if(!match) throw new Error('Unsupported SDK version: '+version);
      const api=match[4] ? +match[4] : +match[1]>=20 ? +match[1] : NaN;
      if (!Number.isFinite(api)) throw new Error('SDK API number is required: '+version);
      return +match[1]*10000000 + +match[2]*100000 + +match[3]*1000 + api;
    }
    const config={app:Object.assign({},app,{debug:true,buildMode:'debug',bundleType:'app',
      minAPIVersion:apiVersion(product.compatibleSdkVersion || '6.1.0(23)'),
      targetAPIVersion:apiVersion(product.targetSdkVersion || '26.0.0(26)'),
      minMinorAPIVersion:0,minPatchAPIVersion:0,targetMinorAPIVersion:0,targetPatchAPIVersion:0,
      apiReleaseType:'Release',compileSdkType:'HarmonyOS',compileSdkVersion:'26.0.0.105'}),
      module:Object.assign({},module,{compileMode:'esmodule',virtualMachine:'ark13.0.1.0',packageName:moduleName})};
    const packInfo={summary:{app:{bundleName:app.bundleName,bundleType:'app',version:{code:app.versionCode,name:app.versionName}},
      modules:[{mainAbility:module.mainElement,deviceType:module.deviceTypes,abilities:module.abilities || [],
        distro:{moduleType:module.type,installationFree:!!module.installationFree,deliveryWithInstall:module.deliveryWithInstall!==false,moduleName:module.name},
        apiVersion:{compatible:config.app.minAPIVersion%1000,releaseType:'Release',target:config.app.targetAPIVersion%1000}}]},
      packages:[{deviceType:module.deviceTypes,moduleType:module.type,deliveryWithInstall:module.deliveryWithInstall!==false,name:module.name+'-default'}]};
    return {ok:true,bundleName:app.bundleName,moduleName:module.name,packInfoJSON:JSON.stringify(packInfo),
      abilityName:module.mainElement,configJSON:JSON.stringify(config),
      modules:profile.modules || [],product:product.name || 'default'};
  }
  for (const base of [root, root + '/' + (options.moduleName || 'entry')]) {
    const pkg = json5.parse(files[base+'/oh-package.json5'] || '{}');
    for (const [name, version] of Object.entries(pkg.dependencies || {})) {
      if (typeof version !== 'string' || !version.startsWith('file:')) continue;
      const target = path.resolve(base,version.slice(5));
      for (const filename of Object.keys(files)) if(filename.startsWith(target+'/')) {
        files[base+'/oh_modules/'+name+filename.slice(target.length)] = files[filename];
      }
    }
  }
  if (typeof __sdkFiles !== 'undefined') {
    Object.assign(__sdkFiles, files);
    for (const name of Object.keys(files)) {
      let child = name;
      while (child !== '/') {
        const parent = path.dirname(child);
        const entries = __sdkDirectories[parent] || (__sdkDirectories[parent] = []);
        if (!entries.includes(path.basename(child))) entries.push(path.basename(child));
        child = parent;
      }
    }
  }
  process.env.compiler = 'on'; process.env.compileTool = 'rollup'; process.env.compileMode = 'moduleJson';
  const main = require(path.join(sdk, 'main.js'));
  const profile = json5.parse(files[root + '/build-profile.json5'] || '{}');
  const product = (profile.app && profile.app.products || [])[0] || {};
  const moduleName = options.moduleName || 'entry';
  const moduleRoot = root + '/' + moduleName;
  const origin = product.compatibleSdkVersion || '6.1.0(23)';
  const apiMatch = /\((\d+)\)/.exec(String(origin));
  const api = typeof origin === 'number' ? origin : apiMatch ? +apiMatch[1] : parseInt(origin, 10);
  if (!Number.isFinite(api)) throw new Error('Unsupported compatibleSdkVersion: ' + origin);
  Object.assign(main.projectConfig, {
    projectPath: moduleRoot + '/src/main/ets', projectRootPath: root, modulePath: moduleRoot,
    moduleRootPath: moduleRoot, etsLoaderPath:sdk, cachePath: root+'/.arkforge-sdk-cache/check', runtimeOS:product.runtimeOS || 'HarmonyOS',
    packageDir:'oh_modules', packageJson:'oh-package.json5', compileMode:'esmodule',
    moduleName, bundleName: options.bundleName, compileSdkVersion:26, compatibleSdkVersion:api,
    originCompatibleSdkVersion:origin, minAPIVersion:api, arkTSVersion:'1.1',
    buildMode:'debug', xtsMode:true, globalModulePaths:main.globalModulePaths,
    aceModuleJsonPath:moduleRoot+'/src/main/module.json5'
  });
  fs.mkdirSync(root+'/.arkforge-sdk-cache', {recursive:true});
  const moduleConfig = (json5.parse(files[moduleRoot+'/src/main/module.json5'] || '{}').module || {});
  const moduleProfile = json5.parse(files[moduleRoot+'/build-profile.json5'] || '{}');
  const arkOptions = Object.assign({},profile.app && profile.app.buildOption && profile.app.buildOption.arkOptions,
    product.buildOption && product.buildOption.arkOptions,moduleProfile.buildOption && moduleProfile.buildOption.arkOptions);
  Object.assign(main.projectConfig,arkOptions);
  main.projectConfig.arkTSVersion = arkOptions.arkTSVersion || main.partialUpdateConfig.arkTSVersion || '1.1';
  if (!['1.0','1.1'].includes(main.projectConfig.arkTSVersion)) throw new Error('Unsupported ArkTS dialect: '+main.projectConfig.arkTSVersion);
  main.projectConfig.strictMode = Object.assign({},profile.app && profile.app.buildOption && profile.app.buildOption.strictMode,
    product.buildOption && product.buildOption.strictMode,moduleProfile.buildOption && moduleProfile.buildOption.strictMode);
  main.projectConfig.deviceTypes = moduleConfig.deviceTypes || ['tablet'];
  main.projectConfig.externalApiPaths = (process.env.externalApiPaths || '').split(path.delimiter).filter(Boolean);
  main.projectConfig.permission = {requestPermissions:moduleConfig.requestPermissions || [],definePermissions:moduleConfig.definePermissions || []};
  const apiChecks = require(path.join(sdk, 'lib/fast_build/system_api/api_check_utils.js'));
  apiChecks.configureSyscapInfo(main.projectConfig);
  apiChecks.configurePermission(main.projectConfig);
  const ts = require(path.join(sdk, 'node_modules/typescript'));
  const checker = require(path.join(sdk, 'lib/ets_checker.js'));
  const roots = Object.keys(files).filter(name => name.startsWith(moduleRoot+'/src/main/ets/') && /(?<!\.d)\.(ets|ts)$/.test(name));
  if (!roots.length) throw new Error('No ArkTS sources to check');
  console.log('[ArkForge] Official SDK input: ' + roots.length + ' source files');
  const emitted = [], raw = [], output = [];
  const lintApi = require(path.join(sdk,'lib/do_arkTS_linter.js'));
  const runLinter = lintApi.doArkTSLinter;
  lintApi.doArkTSLinter = function(){const items=runLinter.apply(this,arguments);raw.push(...items);return items;};
  const capture=(category,text)=>{const value=String(text);output.push(value);if(category!==undefined)emitted.push({category,text:value});};
  const logger = {trace(text){capture(undefined,text);},debug(text){capture(undefined,text);},info(text){capture(undefined,text);},error(text){capture(1,text);},warn(text){capture(0,text);}};
  // This executes DevEco's official property collection, ArkTS rules,
  // semantic checks and diagnostic filtering, not an approximation of them.
  // The official performance mode retains the checker until diagnostic-code extraction finishes.
  // The enclosing ArkWeb document is still discarded after this request.
  main.projectConfig.executionMode = 'performance';
  const started = Date.now();
  checker.serviceChecker(roots, logger, [moduleRoot+'/oh_modules',root+'/oh_modules'], undefined, {projectConfig:main.projectConfig});
  const serviceFinished = Date.now();
  const builder = main.globalProgram.builderProgram;
  raw.push(...builder.getSyntacticDiagnostics(),...builder.getSemanticDiagnostics());
  const diagnosticsFinished = Date.now();
  const mapped = emitted.map(entry => {
    const text = entry.text.replace(/\x1b\[[0-9;]*m/g,'');
    const match = /File: (.*?):(\d+):(\d+)\s*\n\s*([\s\S]*)/.exec(text);
    const fileName=match?match[1]:'',line=match?+match[2]:0,column=match?+match[3]:0;
    const message=(match?match[4]:text.replace(/^ArkTS:(?:ERROR|WARN):?\s*/, '')).trim();
    const original=raw.find(d=>ts.flattenDiagnosticMessageText(d.messageText,'\n').trim()===message &&
      (!d.file || (d.file.fileName===fileName && d.file.getLineAndCharacterOfPosition(d.start||0).line+1===line)));
    return {fileName,line,column,code:original?original.code:0,category:entry.category,message};
  });
  return {ok:!mapped.some(d => d.category === 1), diagnostics:mapped, output:output.join('\n'),
    sdkVersion:'26.0.0.851', sourceFiles:roots.length, timing:{serviceMs:serviceFinished-started,diagnosticsMs:diagnosticsFinished-serviceFinished}, hashCache:typeof __sdkHashMetrics==='undefined'?undefined:__sdkHashMetrics};
};

// Read the same unmodified SDK declarations used by the official checker.
// Project inputs and desktop dependency typings are excluded from this tool.
function lookupAPI(options, assets) {
  const query=String(options.query||'').trim().slice(0,160),needle=query.toLowerCase();
  const allowed=name=>/^\/(?:sdk\/ets\/(?:api|kits|arkts|build-tools\/ets-loader\/declarations)\/|hms\/)/.test(name)&&/\.d\.(?:ts|ets)$/.test(name)&&!name.includes('/node_modules/');
  const provenance={source:'Official SDK declaration files bundled with ArkForge',sdkVersion:'26.0.0.851'};
  if(options.sdk_path) {
    const name=String(options.sdk_path);
    if(!allowed(name)||!Object.hasOwn(assets,name))return {...provenance,results:[],error:'请使用查询结果中的 SDK 文件路径'};
    const lines=assets[name].split('\n'),start=Math.max(1,Math.floor(Number(options.start_line)||1)),end=Math.min(lines.length,start+199,Math.max(start,Number(options.end_line)||start+99));
    return {...provenance,results:[{sdk_path:name,start_line:start,end_line:end,total_lines:lines.length,content:lines.slice(start-1,end).join('\n')}]};
  }
  if(!needle)return {...provenance,results:[],error:'请输入准确的 API、类型或模块名称'};
  const normalized=needle.replace(/[^a-z0-9]/g,''),names=Object.keys(assets).filter(allowed).filter(name=>name.toLowerCase().includes(needle)||assets[name].toLowerCase().includes(needle));
  names.sort((a,b)=>Number(!a.toLowerCase().replace(/[^a-z0-9]/g,'').includes(normalized))-Number(!b.toLowerCase().replace(/[^a-z0-9]/g,'').includes(normalized))||a.localeCompare(b));
  const results=[];
  for(const name of names.slice(0,5)) {
    const lines=assets[name].split('\n'),matches=[];
    for(let i=0;i<lines.length;i++)if(lines[i].toLowerCase().includes(needle))matches.push(i);
    const relevant=matches.filter(i=>!/^\s*(?:\*|\/\/)/.test(lines[i]));
    const at=(relevant.length?relevant:matches).slice(0,2);
    if(!at.length)at.push(0);
    const excerpts=at.map(i=>{const start=Math.max(0,i-16),end=Math.min(lines.length,i+45);return {start_line:start+1,end_line:end,content:lines.slice(start,end).join('\n').slice(0,2000)};});
    results.push({sdk_path:name,total_lines:lines.length,excerpts});
  }
  return {...provenance,query,matched_files:names.length,results,instruction:'按原始声明和 JSDoc 核对参数、@since、@syscap 与设备 API。可传 sdk_path 读取完整相关范围；保持原工程功能，依据真实运行证据判断兼容性。'};
}
exports.lookupAPI=lookupAPI;
