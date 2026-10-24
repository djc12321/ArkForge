const path=require('node:path'),fs=require('node:fs');
const app=path.resolve(__dirname,'..');
const esbuild=require(path.resolve(app,'../../../_vendor/vscode/build/node_modules/esbuild'));
const output=path.join(app,'.workbench-build/extension.js');fs.mkdirSync(path.dirname(output),{recursive:true});
esbuild.buildSync({entryPoints:[path.join(app,'workbench/extension/extension.js')],bundle:true,platform:'browser',format:'cjs',target:'es2022',alias:{minimatch:path.resolve(app,'../../../_vendor/vscode/node_modules/minimatch/minimatch.js')},external:['vscode','path'],outfile:output,logLevel:'warning'});
