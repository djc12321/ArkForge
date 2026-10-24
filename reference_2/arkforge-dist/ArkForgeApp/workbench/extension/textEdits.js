// Accept editor/transport line endings while retaining exact character matching.
// Match uniqueness is checked after normalization before mapping back to the document.
function locateUnique(text, fragment) {
  const normalize=value=>value.replace(/\r+\n/g,'\n');
  const source=normalize(text),needle=normalize(fragment);
  const start=source.indexOf(needle);
  if(!needle||start<0||source.indexOf(needle,start+1)>=0)throw new Error('替换内容未匹配或匹配多处，请重新读取并提供唯一文本');
  function originalOffset(offset) {
    let removed=0;
    for(const match of text.matchAll(/\r+\n/g)) {
      if(match.index-removed>=offset)break;
      removed+=match[0].length-1;
    }
    return offset+removed;
  }
  return {start:originalOffset(start),end:originalOffset(start+needle.length)};
}
function assertPublicMethodsRetained(before, after) {
  const names=text=>new Set([...text.matchAll(/^  (?:(?:public|static|async|override)\s+)*([A-Za-z_$][\w$]*)\s*(?:<[^>\n]+>)?\s*\(/gm)].map(match=>match[1]).filter(name=>!['if','for','while','switch','catch','constructor'].includes(name)));
  const current=names(after),missing=[...names(before)].filter(name=>!current.has(name));
  if(missing.length)throw new Error('请保留当前公开方法：'+missing.join('、')+'。修改实现可精确替换方法体；调整接口时先检查调用方。');
}
module.exports={locateUnique,assertPublicMethodsRetained};
