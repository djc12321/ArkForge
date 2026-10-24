// SDK checker host additions; language rules remain in the official SDK.
process.memoryUsage=function(){return {rss:0,heapUsed:0,heapTotal:0,external:0};};
process.hrtime=function(previous){var n=Date.now()/1000;var v=[Math.floor(n),Math.floor(n%1*1e9)];return previous?[v[0]-previous[0],v[1]-previous[1]]:v;};
process.hrtime.bigint=function(){return BigInt(Date.now())*1000000n;};
__fs.rmSync=__fs.unlinkSync;
var __checkFds={},__checkFd=100;
__fs.openSync=function(path,mode){var fd=++__checkFd;__checkFds[fd]=__path.normalize(path);if(mode==='w')__sdkFiles[__checkFds[fd]]='';return fd;};
__fs.writeSync=function(fd,text){if(!__checkFds[fd])throw new Error('Invalid SDK cache fd');__sdkFiles[__checkFds[fd]]=(__sdkFiles[__checkFds[fd]]||'')+String(text);return text.length;};
__fs.closeSync=function(fd){delete __checkFds[fd];};
__fs.statSync=(function(original){return function(p){p=__path.normalize(p);var s;if(!__fs.existsSync(p)&&(__sdkDirectories[__path.dirname(p)]||[]).includes(__path.basename(p)))s={size:0,mtimeMs:0,isDirectory:function(){return false;},isFile:function(){return true;}};else s=original(p);s.isSymbolicLink=function(){return false;};return s;};})(__fs.statSync);
__fs.lstatSync=__fs.statSync;
__builtins.perf_hooks.performance.mark=function(){};
__builtins.perf_hooks.performance.measure=function(){};
__builtins.perf_hooks.performance.clearMarks=function(){};
__builtins.perf_hooks.performance.clearMeasures=function(){};
// SDK file versions require SHA-256, not a non-cryptographic substitute.
function __sha256(bytes) {
 var k=[0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2];
 var h=[0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19];
 var length=bytes.length,size=Math.ceil((length+9)/64)*64,data=new Uint8Array(size);data.set(bytes);data[length]=128;
 var view=new DataView(data.buffer);view.setUint32(size-8,Math.floor(length/0x20000000));view.setUint32(size-4,length*8);
 function r(x,n){return x>>>n|x<<(32-n);}var w=new Uint32Array(64);
 for(var offset=0;offset<size;offset+=64){for(var i=0;i<16;i++)w[i]=view.getUint32(offset+i*4);for(var i=16;i<64;i++){var x=w[i-15],y=w[i-2];w[i]=w[i-16]+(r(x,7)^r(x,18)^x>>>3)+w[i-7]+(r(y,17)^r(y,19)^y>>>10);}
 var a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],j=h[7];for(var i=0;i<64;i++){var t1=(j+(r(e,6)^r(e,11)^r(e,25))+(e&f^~e&g)+k[i]+w[i])|0,t2=((r(a,2)^r(a,13)^r(a,22))+(a&b^a&c^b&c))|0;j=g;g=f;f=e;e=d+t1|0;d=c;c=b;b=a;a=t1+t2|0;}[a,b,c,d,e,f,g,j].forEach(function(v,i){h[i]=h[i]+v|0;});}
 return h.map(function(v){return (v>>>0).toString(16).padStart(8,'0');}).join('');
}
__builtins.crypto.createHash=function(algorithm){if(algorithm!=='sha256')throw new Error('Unsupported SDK hash '+algorithm);var chunks=[];return {update:function(value){chunks.push(typeof value==='string'?new Uint8Array(__bytes(value)):value);return this;},digest:function(format){if(format!=='hex')throw new Error('Unsupported SDK digest encoding');var n=chunks.reduce(function(n,c){return n+c.length;},0),bytes=new Uint8Array(n),offset=0;chunks.forEach(function(c){bytes.set(c,offset);offset+=c.length;});return __sha256(bytes);}};};
