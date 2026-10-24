const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'..'),raw=path.join(root,'entry/src/main/resources/rawfile');
const pairs=[['compiler/sdk-check.json','compiler/sdk-check.bundle.js'],['codeoss-manifest.json','codeoss.zip'],['backend/signer-build.json','@libsigntool']];
for(const [manifest,asset] of pairs){
 const expected=JSON.parse(fs.readFileSync(path.join(raw,manifest),'utf8')).sha256;
 const file=asset==='@libsigntool'?path.join(root,'entry/libs/arm64-v8a/libsigntool.so'):path.join(raw,asset);
 const actual=crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
 if(typeof expected!=='string'||actual!==expected.toLowerCase())throw Error('Runtime asset SHA-256 mismatch: '+asset);
}
console.log('Runtime assets verified: official SDK, workbench and ARM64 signer.');
