// Resource symbol IDs only; no generated application JS or ABC is exported.
const fs = require('fs'),path = require('path');
const root = path.resolve(process.argv[2]);
const resources = {app:{},sys:require(path.join(process.env.HARMONY_RUNTIME_HOME || 'D:/tools/HarmonyOS/26.0.0.851/command-line-tools','sdk/default/openharmony/ets/build-tools/ets-loader/sysResource.js')).sys};
for (const line of fs.readFileSync(path.join(root,'entry/build/default/intermediates/res/default/ResourceTable.txt'),'utf8').trim().split(/\r?\n/)) {
  const [type,name,id] = line.trim().split(/\s+/);
  (resources.app[type] ||= {})[name] = Number(id);
}
fs.writeFileSync(process.argv[3],JSON.stringify(resources));
console.log('Prepared resource symbol map without application bytecode.');
