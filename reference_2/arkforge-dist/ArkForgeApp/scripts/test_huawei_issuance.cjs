'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const ts=require('C:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/ets/build-tools/ets-loader/node_modules/typescript');
async function scenario(failProfile=false) {
 let profileFailure=failProfile,uuidSequence=0;const assets=new Map();
 let certName,activated=false,password,fd=0;const files=new Map(),directories=new Set(),handles=new Map(),calls=[];
 const asset={Tag:{ALIAS:1,SECRET:2,RETURN_TYPE:3,CONFLICT_RESOLUTION:4},ReturnType:{ALL:1},ConflictResolution:{OVERWRITE:1},query:async attributes=>{const value=assets.get(Buffer.from(attributes.get(1)).toString());return value?[new Map([[2,value]])]:[];},add:async attributes=>{assert(attributes.get(2).length<=1024);assets.set(Buffer.from(attributes.get(1)).toString(),attributes.get(2));},remove:async attributes=>{assets.delete(Buffer.from(attributes.get(1)).toString());}};
 const util={generateRandomUUID:()=> (++uuidSequence).toString(16).padStart(32,'0'),TextEncoder:class {encodeInto(s){return new TextEncoder().encode(s);}},TextDecoder:class {decodeToString(b){return new TextDecoder().decode(b);}}};
 const fileIo={OpenMode:{CREATE:1,WRITE_ONLY:2,TRUNC:4},mkdirSync(p){if(directories.has(p))throw new Error('File exists');directories.add(p);},accessSync:p=>files.has(p)||directories.has(p),readTextSync:p=>files.get(p),unlinkSync:p=>files.delete(p),
  openSync:p=>{handles.set(++fd,p);return {fd};},writeSync:(id,data)=>files.set(handles.get(id),data),closeSync(){}};
 const http={RequestMethod:{GET:'GET',POST:'POST'},HttpDataType:{STRING:1,ARRAY_BUFFER:2},createHttp:()=>({destroy(){},request:async(url,options)=>{
  calls.push({url,body:options.extraData});let value;
  if(url.includes('/jwToken/check'))value={status:true,userInfo:{accessToken:'fixture-access-token'}};
  else if(url.includes('/user-team-list'))value={teams:[{id:'my-team'}]};
  else if(url.includes('/cert/add')){const form=new URLSearchParams(options.extraData);certName=form.get('certName');assert.equal(form.get('csr'),'PUBLIC CSR');value={ret:{code:0}};}
  else if(url.includes('/cert/list'))value={certList:certName?[{id:'new-cert-id',certName,certObjectId:'cert-object-id'}]:[]};
  else if(url.includes('/url/reapply'))value={urlsInfo:[{newUrl:'https://storage.platform.dbankcloud.cn/material'}]};
  else if(url.startsWith('https://storage.platform.dbankcloud.cn/')){assert(!options.header,'download must not receive account headers');return {responseCode:200,result:new Uint8Array([1,2,3]).buffer};}
  else if(url.includes('/device/list'))value={list:[{id:'registered-device',udid:'a'.repeat(64)}]};
  else if(url.includes('/ide/test/provision/add')){
   const request=JSON.parse(options.extraData);assert.equal(request.packageName,'com.test.app');assert.deepEqual(request.certList,['new-cert-id']);assert.deepEqual(request.deviceList,['registered-device']);
   value=profileFailure?{ret:{code:205389938}}:{ret:{code:0},provisionFileUrl:'profile-object-id'};
  } else throw new Error('unexpected endpoint');
  return {responseCode:200,result:JSON.stringify(value)};
 }})};
 const signer={runSigner:async args=>{
  calls.push({command:args[0]});
  if(args[0]==='generate-keypair'){
   password=args[args.indexOf('-keystorePwd')+1];assert.equal(password,args[args.indexOf('-keyPwd')+1]);files.set(args[args.indexOf('-keystoreFile')+1],'PRIVATE KEY');
  } else {assert.equal(args[0],'generate-csr');files.set(args[args.indexOf('-outFile')+1],'PUBLIC CSR');}
  return 0;
 }};
 const SigningMaterials={importFiles:async(context,uris,credentials,bundle)=>{
  assert.equal(bundle,'com.test.app');assert.equal(credentials.storePassword,password);assert(uris.every(p=>files.has(p)));activated=true;
 }};
 const sandbox={exports:{},Date:class extends Date {static now(){return 1792800000000;}},Map,JSON,Uint8Array,
  require:name=>name.includes('AssetStore')?{asset}:name.includes('ArkTS')?{util}:name.includes('Network')?{http}:name.includes('CoreFile')?{fileIo}:
    name==='libsigntool.so'?{default:signer}:name.includes('SigningMaterials')?{SigningMaterials}:{}};
 vm.runInNewContext(ts.transpileModule(fs.readFileSync('entry/src/main/ets/services/HuaweiSigning.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2020}}).outputText,sandbox);
 const service=sandbox.exports.HuaweiSigning;await service.save({jwt:'fixture-jwt',access:'fixture-access-token',uid:'42',name:'Developer'});
 const operation=service.issue({tempDir:'/sandbox/temp',filesDir:'/sandbox/files'},'com.test.app','my-team','a'.repeat(64),()=>{});
 if(failProfile){
  await assert.rejects(operation,/205389938/);assert.equal(activated,false);assert([...files.keys()].some(p=>p.endsWith('/key.p12')),'failed attempts retain their private key securely for retry');
  profileFailure=false;assert.equal(JSON.parse(await service.issue({filesDir:'/sandbox/files'},'com.test.app','my-team','a'.repeat(64),()=>{})).bundleName,'com.test.app');
  assert.equal(calls.filter(x=>x.command==='generate-keypair').length,1,'retry must reuse the pending key');assert.equal(calls.filter(x=>x.url?.includes('/cert/add')).length,1,'retry must reuse its existing certificate');
 }else assert.equal(JSON.parse(await operation).bundleName,'com.test.app');
 assert.equal(activated,true);assert.equal(files.size,0,'temporary private key and downloaded materials must be removed');
 assert(!calls.some(x=>x.url?.includes('/delete')),'do not delete existing cloud resources');
 assert(!calls.some(x=>x.url?.includes('/device/add')),'reuse an already registered device');
 assert(calls.filter(x=>x.body).every(x=>!x.body.includes('PRIVATE KEY')&&!x.body.includes(password)),'private key/password must never reach cloud APIs');
}
(async()=>{await scenario();await scenario(true);console.log('PASS: local key/CSR, cloud certificate/device/profile flow, no private-key upload, existing-device reuse and no activation after cloud failure.');})().catch(error=>{console.error(error);process.exitCode=1;});
