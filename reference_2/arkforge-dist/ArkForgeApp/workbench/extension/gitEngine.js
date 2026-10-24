function words(line) {
  const args=[];let word='',quote='',started=false;
  for(let i=0;i<line.length;i++) {
    const c=line[i];
    if(c==='\\' && quote!=="'"){if(++i===line.length)throw new Error('命令末尾缺少转义字符');word+=line[i];started=true;}
    else if(quote){if(c===quote)quote='';else word+=c;}
    else if(c==='"'||c==="'"){quote=c;started=true;}
    else if(/\s/.test(c)){if(started){args.push(word);word='';started=false;}}
    else {if(/[;&|<>`]/.test(c))throw new Error('此终端不执行 Shell 管道或脚本，请分别输入 Git 命令');word+=c;started=true;}
  }
  if(quote)throw new Error('引号未闭合');if(started)args.push(word);return args;
}
function path(value,base='/workspace') {
  if(value.includes('\\')||value.includes('\0'))throw new Error('无效路径');
  const parts=(value.startsWith('/')?value:base+'/'+value).split('/'),out=[];
  for(const part of parts){if(!part||part==='.')continue;if(part==='..'){if(out.length<=1)throw new Error('路径不能离开当前工程');out.pop();}else out.push(part);}
  if(out[0]!=='workspace')throw new Error('路径不能离开当前工程');return '/'+out.join('/');
}
exports.createGitEngine=function(native) {
  let cwd='/workspace';
  return {get cwd(){return cwd;},async execute(line){
    const args=words(line);if(!args.length)return {output:'',code:0};
    const command=args[0];
    if(command==='git'||command==='help') {
      const argv=command==='help'?['git','--help']:args;
      return JSON.parse(await native('git_run',{path:cwd,args:argv}));
    }
    if(command==='pwd')return {output:cwd+'\n',code:0};
    if(command==='clear')return {output:'\x1b[2J\x1b[H',code:0};
    if(command==='cd'){
      const next=path(args[1]||'/workspace',cwd),stat=JSON.parse(await native('fs.stat',{path:next}));
      if(stat.type!==2)throw new Error('[ArkForge] 目标不是目录');cwd=next;return {output:'',code:0};
    }
    if(command==='ls')return {output:JSON.parse(await native('fs.list',{path:path(args[1]||'.',cwd)})).map(row=>row[0]).join('\n')+'\n',code:0};
    throw new Error('[ArkForge] 此终端转发官方 Git 命令；系统 Shell 尚未接入');
  }};
};
exports.words=words;
