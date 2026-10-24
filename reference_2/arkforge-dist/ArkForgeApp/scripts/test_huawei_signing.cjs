'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const ts=require('C:/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/ets/build-tools/ets-loader/node_modules/typescript');
let secret,uuidSequence=0,failAfter=-1;const assets=new Map();
let callback={pending:true},queue=[],opened=[],requests=[],time=Date.UTC(2026,9,24),cancelled=0;
const asset={Tag:{ALIAS:1,SECRET:2,RETURN_TYPE:3,CONFLICT_RESOLUTION:4},ReturnType:{ALL:1},ConflictResolution:{OVERWRITE:1},
  query:async map=>{const value=assets.get(Buffer.from(map.get(1)).toString());if(!value)throw {code:24000002};return [new Map([[2,value]])];},add:async map=>{const key=Buffer.from(map.get(1)).toString(),value=map.get(2);assert(value.length<=1024);if(failAfter===0){failAfter=-1;throw new Error('simulated storage failure');}if(failAfter>0)failAfter--;assets.set(key,value);if(key==='ArkForge.Huawei.Developer')secret=value;},remove:async map=>{const key=Buffer.from(map.get(1)).toString();assets.delete(key);if(key==='ArkForge.Huawei.Developer')secret=undefined;}};
const http={RequestMethod:{GET:'GET',POST:'POST',DELETE:'DELETE'},HttpDataType:{STRING:1,ARRAY_BUFFER:2},createHttp:()=>({request:async(url,options)=>{
 requests.push({url,options});assert(queue.length,'unexpected request');const reply=queue.shift();if(reply instanceof Error)throw reply;
 return {responseCode:200,result:typeof reply==='string'?reply:JSON.stringify(reply)};
},destroy(){}})};
const resources={huaweiCallbackDiagnostics:()=>'{}',startHuaweiCallback:code=>{assert.equal(code.length,32);return 18777;},takeHuaweiCallback:()=>{const value=callback;callback={pending:true};return JSON.stringify(value);},cancelHuaweiCallback:()=>cancelled++};
const util={generateRandomUUID:()=> (++uuidSequence).toString(16).padStart(32,'0'),TextEncoder:class {encodeInto(s){return new TextEncoder().encode(s);}},TextDecoder:class {decodeToString(b){return new TextDecoder().decode(b);}},Base64Helper:class {decodeSync(s){return Buffer.from(s,'base64');}}};
const sandbox={exports:{},Date:class extends Date {static now(){return time;}},Map,JSON,Uint8Array,
 require:name=>name.includes('AssetStore')?{asset}:name.includes('ArkTS')?{util}:name.includes('Network')?{http}:name==='libarkforge_resources.so'?{default:resources}:name.includes('AtomicFiles')?{AtomicFiles:{write(){}}}:name.includes('LocalDevice')?{LocalDevice:{signingUdid:async()=> 'a'.repeat(64)}}:{}};
vm.runInNewContext(ts.transpileModule(fs.readFileSync('entry/src/main/ets/services/HuaweiSigning.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS,target:ts.ScriptTarget.ES2020}}).outputText,sandbox);
const service=sandbox.exports.HuaweiSigning,context={openLink:async url=>opened.push(url),startAbility:async()=>{},applicationInfo:{name:'com.test.ide'}};
const call=async(action,args={})=>JSON.parse(await service.execute(context,action,JSON.stringify(args)));
(async()=>{
 assert.equal((await call('status')).signedIn,false);
 let flow=await call('start');assert.match(opened[0],/^https:\/\/cn.devecostudio.huawei.com\/console\/DevEcoIDE\/apply\?port=18777&appid=1007&code=[a-f0-9]{32}$/);
 assert.equal((await call('poll',{id:flow.id})).state,'pending');assert.equal(requests.length,0);
 callback={tempToken:'fake-temporary-token',siteId:'1'};
 const jwt='header.'+Buffer.from(JSON.stringify({userName:'Developer',userId:'42',padding:'x'.repeat(1900)})).toString('base64url')+'.signature';
 queue.push(jwt,{status:true,userInfo:{accessToken:'fake-access-token'}});
 const result=await call('poll',{id:flow.id});assert.equal(result.state,'authenticated');assert.equal(result.name,'Developer');
 assert(!JSON.stringify(result).includes('fake-'));assert(secret);assert.equal((await call('status')).signedIn,true);
 assert(requests[0].url.includes('tempToken=fake-temporary-token'));assert.equal(requests[1].options.header.jwtToken,jwt);
 queue.push({status:true,userInfo:{accessToken:'refreshed-token'}},{teams:[{id:'team-1',name:'My Team'}]});
 const prepared=await call('prepare');assert.equal(prepared.udid.length,64);assert.equal(prepared.teams[0].id,'team-1');assert(!JSON.stringify(prepared).includes('token'));
 queue.push({status:true,userInfo:{accessToken:'refreshed-token'}},{teams:[{id:'team-1'}]},{certList:[{id:'keep-id',certName:'keep.cer'},{id:'selected-id',certName:'selected.cer'}]},{ret:{code:0}});
 await call('revokeCertificate',{teamId:'team-1',certName:'selected.cer'});
 assert.equal(requests.at(-1).options.method,'DELETE');assert.deepEqual(JSON.parse(requests.at(-1).options.extraData),{certIds:['selected-id']});
 const deletes=requests.filter(x=>x.options.method==='DELETE').length;
 queue.push({status:true,userInfo:{accessToken:'refreshed-token'}},{teams:[{id:'team-1'}]},{certList:[{id:'keep-id',certName:'keep.cer'}]});
 await assert.rejects(call('revokeCertificate',{teamId:'team-1',certName:'missing.cer'}));assert.equal(requests.filter(x=>x.options.method==='DELETE').length,deletes);
 const originalCount=assets.size;failAfter=1;await assert.rejects(service.save({jwt:'x'.repeat(2500),access:'new',uid:'new',name:'Changed'}));
 assert.equal((await service.saved()).name,'Developer','failed chunk write must preserve the previous account');assert.equal(assets.size,originalCount,'failed writes must remove partial encrypted chunks');
 assert(assets.size>3,'long tokens must use multiple encrypted chunks');
 await call('logout');assert.equal(secret,undefined);assert.equal(assets.size,0);
 flow=await call('start');await call('cancel',{id:flow.id});assert.equal((await call('poll',{id:flow.id})).state,'cancelled');
 flow=await call('start');time+=600001;await assert.rejects(call('poll',{id:flow.id}),/超时/);
 flow=await call('start');callback={tempToken:'secret-test-probe'};queue.push(new Error('network error URL contains secret-test-probe'));
 try{await call('poll',{id:flow.id});assert.fail('expected network failure');}catch(error){assert(!error.message.includes('secret-test-probe'));}
 assert.equal(secret,undefined);assert(cancelled>0);
 console.log('PASS: official browser URL, native-only token exchange, AssetStore, refresh/team/device preparation, cancellation, expiry and secret-free errors.');
})().catch(error=>{console.error(error);process.exitCode=1;});
