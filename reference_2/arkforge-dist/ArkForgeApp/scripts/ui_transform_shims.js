// Virtual POSIX filesystem and CommonJS host for the official SDK transformer.
var __moduleCache = Object.create(null);
var global=globalThis;
var __path = {
  sep: '/', delimiter: ':',
  normalize: function(p) {
    p=String(p).replace(/\\/g,'/'); var absolute=p[0]==='/', parts=[];
    p.split('/').forEach(function(v){if(!v||v==='.')return;if(v==='..'&&parts.length&&parts[parts.length-1]!=='..')parts.pop();else if(v!=='..'||!absolute)parts.push(v);});
    return (absolute?'/':'')+parts.join('/') || (absolute?'/':'.');
  },
  join: function(){return __path.normalize(Array.prototype.join.call(arguments,'/'));},
  resolve: function(){var p='';for(var i=arguments.length-1;i>=0;i--){p=String(arguments[i])+'/'+p;if(String(arguments[i])[0]==='/')break;}return __path.normalize(p[0]==='/'?p:'/'+p);},
  dirname: function(p){p=__path.normalize(p);var i=p.lastIndexOf('/');return i<0?'.':i===0?'/':p.slice(0,i);},
  basename: function(p,suffix){p=String(p).replace(/\/$/,'').split('/').pop();return suffix&&p.endsWith(suffix)?p.slice(0,-suffix.length):p;},
  extname: function(p){p=__path.basename(p);var i=p.lastIndexOf('.');return i>0?p.slice(i):'';},
  isAbsolute: function(p){return p[0]==='/';},
  relative: function(a,b){a=__path.resolve(a).split('/');b=__path.resolve(b).split('/');while(a.length&&b.length&&a[0]===b[0]){a.shift();b.shift();}return a.map(function(){return'..';}).concat(b).join('/');},
  parse: function(p){return {root:p[0]==='/'?'/':'',dir:__path.dirname(p),base:__path.basename(p),ext:__path.extname(p),name:__path.basename(p,__path.extname(p))};}
};
__path.posix=__path;__path.win32=__path;
function __bytes(text){var s=unescape(encodeURIComponent(text)),a=[];for(var i=0;i<s.length;i++)a.push(s.charCodeAt(i));return a;}
function __text(bytes){var s='';for(var i=0;i<bytes.length;i++)s+=String.fromCharCode(bytes[i]);return decodeURIComponent(escape(s));}
function Buffer(value){var result=new Uint8Array(typeof value==='number'?value:typeof value==='string'?__bytes(value):value);result.toString=function(){return __text(this);};return result;}
Buffer.from=function(value){return Buffer(value);};Buffer.alloc=function(size){return Buffer(size);};Buffer.isBuffer=function(value){return value instanceof Uint8Array;};Buffer.byteLength=function(s){return __bytes(s).length;};
// Compatibility feature gates; this host does not provide a Node process.
var process={env:{},platform:'linux',arch:'arm64',version:'v18.0.0-arkforge',versions:{node:'18.0.0'},argv:[],cwd:function(){return'/project';},nextTick:function(callback){callback();},stdout:{write:function(s){console.log(s);}},stderr:{write:function(s){console.error(s);}},exit:function(code){throw new Error('Compiler requested process.exit('+code+')');}};
function __missing(name){return function(){throw new Error('Unsupported compiler host operation: '+name);};}
if(typeof setTimeout==='undefined')globalThis.setTimeout=__missing('setTimeout');
// Cancelling a nonexistent timer is valid; scheduling still fails closed in
// the synchronous compiler host rather than pretending to run async work.
if(typeof clearTimeout==='undefined')globalThis.clearTimeout=function(){};
var __fs={
  existsSync:function(p){p=__path.normalize(p);return Object.prototype.hasOwnProperty.call(__sdkFiles,p)||Object.prototype.hasOwnProperty.call(__sdkDirectories,p);},
  readFileSync:function(p,encoding){p=__path.normalize(p);if(!Object.prototype.hasOwnProperty.call(__sdkFiles,p))throw new Error('ENOENT: '+p);return encoding?__sdkFiles[p]:Buffer.from(__sdkFiles[p]);},
  readdirSync:function(p){p=__path.normalize(p);if(!Object.prototype.hasOwnProperty.call(__sdkDirectories,p))throw new Error('ENOENT directory: '+p);return __sdkDirectories[p].slice();},
  statSync:function(p){p=__path.normalize(p);if(!__fs.existsSync(p))throw new Error('ENOENT stat: '+p);return {size:(__sdkFiles[p]||'').length,mtimeMs:0,isDirectory:function(){return Object.prototype.hasOwnProperty.call(__sdkDirectories,p);},isFile:function(){return Object.prototype.hasOwnProperty.call(__sdkFiles,p);}};},
  realpathSync:function(p){return __path.resolve(p);},
  mkdirSync:function(p){__sdkDirectories[__path.normalize(p)]=[];},
  writeFileSync:function(p,text){__sdkFiles[__path.normalize(p)]=String(text);},
  unlinkSync:function(p){delete __sdkFiles[__path.normalize(p)];}
};
__fs.lstatSync=__fs.statSync;__fs.realpathSync.native=__fs.realpathSync;
['open','close','read','write','fstat','readFile','writeFile','readdir','lstat','stat','unlink','link','symlink','rename','mkdir','rmdir','chmod','lchmod','fchmod','chown','lchown','fchown','readlink','realpath','access','utimes','futimes','copyFile'].forEach(function(name){__fs[name]=__missing('fs.'+name);});
__fs.constants={O_RDONLY:0};
var __assert=function(condition,message){if(!condition)throw new Error(message||'Assertion failed');};
__assert.ok=__assert;__assert.strictEqual=function(a,b){__assert(a===b,'Expected '+a+' === '+b);};
var __logger={trace:function(){},debug:function(){},info:function(){console.log.apply(console,arguments);},warn:function(){console.warn.apply(console,arguments);},error:function(){console.error.apply(console,arguments);},fatal:function(){console.error.apply(console,arguments);},isDebugEnabled:function(){return false;}};
function EventEmitter(){this.listeners={};}
EventEmitter.prototype.on=function(name,fn){(this.listeners[name]||(this.listeners[name]=[])).push(fn);return this;};
EventEmitter.prototype.once=function(name,fn){var self=this;function once(){self.removeListener(name,once);fn.apply(self,arguments);}return this.on(name,once);};
EventEmitter.prototype.removeListener=function(name,fn){this.listeners[name]=(this.listeners[name]||[]).filter(function(v){return v!==fn;});return this;};
EventEmitter.prototype.emit=function(name){var args=Array.prototype.slice.call(arguments,1);(this.listeners[name]||[]).slice().forEach(function(fn){fn.apply(null,args);});return true;};
EventEmitter.EventEmitter=EventEmitter;
var __cluster=new EventEmitter();__cluster.isPrimary=true;__cluster.isMaster=true;__cluster.workers={};__cluster.fork=__missing('cluster.fork');
function Stream(){EventEmitter.call(this);}Stream.prototype=Object.create(EventEmitter.prototype);Stream.Readable=Stream;Stream.Writable=Stream;Stream.Transform=Stream;Stream.PassThrough=Stream;Stream.Duplex=Stream;
var __builtins={
  fs:__fs,path:__path,os:{platform:function(){return'linux';},type:function(){return'Linux';},homedir:function(){return'/sandbox';},tmpdir:function(){return'/tmp';},EOL:'\n',cpus:function(){return[{}];}},
  assert:__assert,buffer:{Buffer:Buffer},crypto:{createHash:__missing('crypto.createHash'),randomBytes:__missing('crypto.randomBytes')},
  cluster:__cluster,constants:{O_RDONLY:0},events:EventEmitter,stream:Stream,
  worker_threads:{isMainThread:true,Worker:__missing('worker_threads.Worker')},
  child_process:{spawn:__missing('child_process.spawn'),exec:__missing('child_process.exec'),execSync:__missing('child_process.execSync'),fork:__missing('child_process.fork')},
  vm:{runInThisContext:__missing('vm.runInThisContext')},module:{},
  log4js:{getLogger:function(){return __logger;},configure:function(){}},
  util:{inspect:function(value){return JSON.stringify(value);},format:function(value){return String(value);},inherits:function(child,parent){child.super_=parent;Object.setPrototypeOf(child.prototype,parent.prototype);}},
  perf_hooks:{performance:{now:function(){return Date.now();}}},
};
function __load(id){
  if(__moduleCache[id])return __moduleCache[id].exports;
  if(__builtins[id])return __builtins[id];
  var body=__moduleBodies[id];if(!body)throw new Error('Compiler module not bundled: '+id);
  var module={exports:{},filename:id,id:id,loaded:false};__moduleCache[id]=module;
  var require=function(request){
    if(__builtins[request])return __builtins[request];
    if(request.indexOf('webpack/')===0||request.indexOf('webpack-sources/')===0)return function DesktopBundlerUnavailable(){throw new Error('Desktop bundler is unavailable in device transformer');};
    var target=(__moduleMap[id]||{})[request];
    if(!target&&request[0]!=='.'&&request[0]!=='/') {
      var directory=__path.dirname(id);
      while(true){var base=__path.join(directory,'node_modules',request),pkg=__sdkFiles[base+'/package.json'];
        if(pkg){var main=JSON.parse(pkg).main||'index.js',candidate=__path.resolve(base,main);if(__moduleBodies[candidate]){target=candidate;break;}if(__moduleBodies[candidate+'.js']){target=candidate+'.js';break;}}
        if(__moduleBodies[base+'/index.js']){target=base+'/index.js';break;}
        if(directory==='/')break;directory=__path.dirname(directory);
      }
    }
    if(!target&&request[0]==='/')target=request;
    if(!target&&request[0]==='.')target=__path.resolve(__path.dirname(id),request);
    if(target&&!__moduleBodies[target]&&!__moduleCache[target]){if(__moduleBodies[target+'.js'])target+='.js';else if(__moduleBodies[target+'/index.js'])target+='/index.js';}
    if(!target)throw new Error('Unresolved compiler dependency '+request+' from '+id);
    return __load(target);
  };
  require.cache=__moduleCache;require.resolve=function(request){return (__moduleMap[id]||{})[request]||__path.resolve(__path.dirname(id),request);};
  body(require,module,module.exports,id,__path.dirname(id));module.loaded=true;return module.exports;
}
