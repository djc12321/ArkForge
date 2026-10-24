'use strict';
const fs=require('fs'),path=require('path'),crypto=require('crypto');
const app=path.resolve(__dirname,'..'),vendor=path.resolve(app,'../../../_vendor');
const digest=crypto.createHash('sha256');
function scan(root,base=root) {
  for(const item of fs.readdirSync(root,{withFileTypes:true}).sort((a,b)=>a.name.localeCompare(b.name))) {
    if(item.name==='.git' || item.name==='test' || item.name==='tests')continue;
    const file=path.join(root,item.name);
    if(item.isDirectory())scan(file,base);
    else if(/\.(c|cpp|h|hpp|txt|prebuilt)$/.test(item.name))digest.update(path.relative(base,file).replace(/\\/g,'/')+'\0').update(fs.readFileSync(file));
  }
}
for(const name of ['developtools_global_resource_tool','libpng','securec','cJSON','zlib']) {
  digest.update(name+'\0');scan(path.join(vendor,name));
}
scan(path.join(app,'native/resources'));
for(const name of ['id_defined.json','system-resources.json'])digest.update(fs.readFileSync(path.join(app,'entry/src/main/resources/rawfile/compiler',name)));
fs.writeFileSync(path.join(app,'entry/src/main/resources/rawfile/compiler/resource-tool.json'),JSON.stringify({sha256:digest.digest('hex'),upstream:'openharmony/developtools_global_resource_tool'}));
