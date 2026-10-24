const vscode=require('vscode');
const minimatch=require('minimatch');
exports.registerProjectSearch=function(context,native) {
  async function files(options,token) {
    const names=JSON.parse(await native('fs.index',{path:options.folder.path}));
    return names.filter(name=>!token.isCancellationRequested &&
      (!(options.includes||[]).length || options.includes.some(glob=>minimatch(name,glob,{dot:true}))) &&
      !(options.excludes||[]).some(glob=>minimatch(name,glob,{dot:true})));
  }
  context.subscriptions.push(vscode.workspace.registerFileSearchProvider('arkforge',{
    provideFileSearchResults:async(query,options,token)=>{
      const pattern=query.pattern.toLowerCase();
      const names=(await files(options,token)).filter(name=>{
        let index=0;for(const c of name.toLowerCase())if(c===pattern[index])index++;
        return index===pattern.length;
      });
      return names.slice(0,options.maxResults??20000).map(name=>vscode.Uri.joinPath(options.folder,name));
    }
  }));
  context.subscriptions.push(vscode.workspace.registerTextSearchProvider('arkforge',{
    provideTextSearchResults:async(query,options,progress,token)=>{
      if(!query.pattern)return {limitHit:false};
      const pattern=query.isRegExp?query.pattern:query.pattern.replace(/[.*+?^${}()|[\]\\]/g,'\\$&');
      const regex=new RegExp(pattern,'gm'+(query.isCaseSensitive?'':'i')+'u');
      let count=0,unreadable=0;
      const max=options.maxResults??10000;
      const names=(await files(options,token)).filter(name=>!/\.(png|jpe?g|gif|webp|so|zip|hap|abc|wasm|ttf|woff2?)$/i.test(name));
      async function* contents() {
        for(let offset=0;offset<names.length && !token.isCancellationRequested;) {
          const batch=JSON.parse(await native('fs.readBatch',{path:options.folder.path,files:names.slice(offset,offset+128),maxFileSize:options.maxFileSize,pattern:regex.source,matchCase:query.isCaseSensitive}));
          if(!batch.consumed)throw new Error('请缩小搜索范围后重试');
          offset+=batch.consumed;unreadable+=batch.unreadable;
          for(const item of batch.items)yield item;
        }
      }
      for await(const {name,text} of contents()) {
        if(token.isCancellationRequested)break;
        const uri=vscode.Uri.joinPath(options.folder,name);
        const starts=[0];for(let i=0;i<text.length;i++)if(text[i]==='\n')starts.push(i+1);
        const position=offset=>{
          let lo=0,hi=starts.length;
          while(lo+1<hi){const mid=(lo+hi)>>1;if(starts[mid]<=offset)lo=mid;else hi=mid;}
          return {line:lo,column:offset-starts[lo]};
        };
        regex.lastIndex=0;let match;
        while((match=regex.exec(text)) && !token.isCancellationRequested) {
          const start=match.index,end=start+match[0].length;
          if(!match[0].length)regex.lastIndex+=text.codePointAt(regex.lastIndex)>65535?2:1;
          if(query.isWordMatch && (/[\p{L}\p{N}_]$/u.test(text.slice(Math.max(0,start-2),start))||/^[\p{L}\p{N}_]/u.test(text.slice(end,end+2))))continue;
          if(count>=max)return {limitHit:true};
          const a=position(start),b=position(end);
          const previewStart=Math.max(starts[a.line],start-80);
          const previewEnd=Math.min(starts[b.line+1]??text.length,Math.max(end,previewStart+(options.previewOptions?.charsPerLine||200)));
          const preview=text.slice(previewStart,previewEnd).replace(/\r?\n$/,'');
          progress.report({uri,ranges:new vscode.Range(a.line,a.column,b.line,b.column),preview:{text:preview,
            matches:new vscode.Range(0,start-previewStart,b.line-a.line,b.line===a.line?end-previewStart:b.column)}});
          count++;
        }
      }
      return {limitHit:false,...(unreadable?{message:{text:unreadable+' 个文件需要读取权限，请检查文件权限后重试。',type:2}}:{})};
    }
  }));
};
