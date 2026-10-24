const vscode=require('vscode');
const {createGitEngine}=require('./gitEngine');
exports.registerGitTerminal=function(context,native) {
  function pty() {
    const write=new vscode.EventEmitter(),close=new vscode.EventEmitter();
    const engine=createGitEngine(native);let line='',busy=false,closed=false,history=[],index=0;
    const output=text=>{if(!closed)write.fire(text);};
    const prompt=()=>output('\x1b[36m'+engine.cwd+'\x1b[0m $ ');
    const run=async command=>{
      busy=true;
      try {
        const result=await engine.execute(command);
        if(result.output){
          if(result.code)output('\x1b[31m');
          output(result.output.replace(/\r?\n/g,'\r\n'));
          if(result.code)output('\x1b[0m');
        }
      } catch(error){output('\x1b[31m[ArkForge] '+String(error.message).replace(/[\x00-\x1f\x7f]/g,' ')+'\x1b[0m\r\n');}
      finally{busy=false;prompt();}
    };
    return {onDidWrite:write.event,onDidClose:close.event,
      open(){output('[ArkForge] Official Git CLI · git --help\r\n');prompt();},
      close(){closed=true;write.dispose();close.dispose();},
      handleInput(data){
        if(closed)return;
        if(busy){if(data==='\x03')output('任务正在执行，请等待完成。\r\n');return;}
        if(data==='\x1b[A'||data==='\x1b[B'){
          index=Math.max(0,Math.min(history.length,index+(data==='\x1b[A'?-1:1)));
          line=history[index]||'';output('\r\x1b[2K');prompt();output(line);return;
        }
        // A pasted script is never executed implicitly as multiple commands.
        if(data.length>1&&/[\r\n]/.test(data)){output('\r\n请逐条输入命令。\r\n');prompt();line='';return;}
        for(const c of data){
          if(c==='\r'||c==='\n'){output('\r\n');const command=line;line='';if(command.trim()){history.push(command);index=history.length;run(command);}else prompt();break;}
          if(c==='\x03'){line='';output('^C\r\n');prompt();}
          else if(c==='\x7f'||c==='\b'){if(line){line=Array.from(line).slice(0,-1).join('');output('\b \b');}}
          else if(c>=' '&&c!=='\x7f'){line+=c;output(c);}
        }
      }};
  }
  context.subscriptions.push(vscode.window.registerTerminalProfileProvider('arkforge.gitTerminal',{
    provideTerminalProfile:()=>new vscode.TerminalProfile({name:'ArkForge 终端',pty:pty()})
  }));
  context.subscriptions.push(vscode.commands.registerCommand('arkforge.openGitTerminal',()=>{
    const terminal=vscode.window.createTerminal({name:'ArkForge 终端',pty:pty()});context.subscriptions.push(terminal);terminal.show();return terminal;
  }));
};
