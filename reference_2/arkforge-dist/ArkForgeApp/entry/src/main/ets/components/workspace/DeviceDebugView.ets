import { DebugState, DebugFrame, DebugVariable } from '../../services/DeviceDebugger';

@Component
export struct DeviceDebugView {
  @Prop state: DebugState = {connected:false,paused:false,status:'尚未连接',frames:[],variables:[]};
  onAction: (method: string) => void = () => {};
  onFrame: (frame: DebugFrame) => void = () => {};
  build() {
    Column() {
      Row({space:8}) {
        Button('继续').enabled(this.state.paused).onClick(() => this.onAction('Debugger.resume'))
        Button('暂停').enabled(this.state.connected && !this.state.paused).onClick(() => this.onAction('Debugger.pause'))
        Button('步过').enabled(this.state.paused).onClick(() => this.onAction('Debugger.stepOver'))
        Button('步入').enabled(this.state.paused).onClick(() => this.onAction('Debugger.stepInto'))
        Button('步出').enabled(this.state.paused).onClick(() => this.onAction('Debugger.stepOut'))
        Button('结束调试').enabled(this.state.connected).onClick(() => this.onAction('stop'))
      }.padding(8)
      Text(this.state.status).fontSize(12).width('100%').padding(6)
      Row() {
        List() {
          ForEach(this.state.frames, (frame: DebugFrame) => {
            ListItem() {
              Text((frame.functionName || '(匿名函数)') + '\n' + (frame.source ? frame.source.path + ':' + frame.source.line : frame.url + ':' + (frame.location.lineNumber + 1) + '（生成代码）'))
                .fontSize(12).fontFamily('monospace').padding(8).width('100%')
                .onClick(() => this.onFrame(frame))
            }
          }, (frame: DebugFrame) => frame.callFrameId)
        }.layoutWeight(1).height('100%')
        List() {
          ForEach(this.state.variables, (variable: DebugVariable) => {
            ListItem() { Text(variable.name + ' = ' + variable.value).fontSize(12).fontFamily('monospace').padding(6).width('100%') }
          }, (variable: DebugVariable) => variable.name)
        }.layoutWeight(1).height('100%')
      }.layoutWeight(1)
    }.width('100%').height('100%')
  }
}
