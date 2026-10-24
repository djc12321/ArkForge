/**
 * DynamicRenderer.ets —— 把 UiNode 树递归渲染为真实 ArkUI 组件
 *
 * 这是「写 JS → 立刻在本机看到界面」的关键：
 * JS 侧只是描述一棵 JSON 树，真正的渲染由 ArkUI 完成，
 * 因此效果与原生组件完全一致。
 */
import { UiNode, UiProps, isContainer } from '../model/UiNode';

@Component
export struct DynamicRenderer {
  @Prop node: UiNode | null = null;

  private prop(props: UiProps | undefined, key: string): string | number | undefined {
    if (!props) return undefined;
    switch (key) {
      case 'size': return props.size;
      case 'color': return props.color;
      case 'weight': return props.weight;
      case 'margin': return props.margin;
      case 'placeholder': return props.placeholder;
      case 'src': return props.src;
      case 'width': return props.width;
      case 'height': return props.height;
      case 'space': return props.space;
      case 'padding': return props.padding;
      case 'align': return props.align;
      default: return undefined;
    }
  }

  /** 读取数值属性 */
  private num(props: UiProps | undefined, key: string, def: number): number {
    const v = this.prop(props, key);
    return (typeof v === 'number') ? v : def;
  }

  /** 读取字符串属性 */
  private str(props: UiProps | undefined, key: string, def: string): string {
    const v = this.prop(props, key);
    return (typeof v === 'string') ? v : def;
  }

  /**
   * 当遇到不受支持的组件类型时，降级展示为一段说明文本，
   * 而不是让整个渲染崩溃——Playground 需要容错。
   */
  @Builder
  private fallback(node: UiNode) {
    Text(`未支持的组件：${node.type}`)
      .fontSize(12)
      .fontColor('#B3261E')
      .backgroundColor('#FDECEA')
      .padding(6)
      .borderRadius(6)
  }

  @Builder
  private renderChildren(node: UiNode) {
    ForEach(node.children ?? [], (child: UiNode) => {
      this.renderNode(child)
    }, (child: UiNode, idx: number) => `${child.type}_${idx}`)
  }

  @Builder
  private renderNode(node: UiNode) {
    if (node.type === 'Text') {
      Text(node.value ?? '')
        .fontSize(this.num(node.props, 'size', 15))
        .fontColor(this.str(node.props, 'color', '#16181D'))
        .fontWeight(this.str(node.props, 'weight', 'normal') === 'bold' ? FontWeight.Bold : FontWeight.Normal)
        .margin(this.num(node.props, 'margin', 2))
    } else if (node.type === 'Button') {
      Button(node.value ?? '按钮')
        .fontSize(this.num(node.props, 'size', 15))
    } else if (node.type === 'Input') {
      TextInput({ placeholder: this.str(node.props, 'placeholder', '请输入') })
        .width('90%')
        .fontSize(this.num(node.props, 'size', 15))
    } else if (node.type === 'Image') {
      Image(this.str(node.props, 'src', ''))
        .width(this.num(node.props, 'width', 120))
        .height(this.num(node.props, 'height', 120))
    } else if (node.type === 'Column') {
      Column({ space: this.num(node.props, 'space', 6) }) {
        this.renderChildren(node)
      }
      .width('100%')
      .padding(this.num(node.props, 'padding', 8))
      .alignItems(this.str(node.props, 'align', 'center') === 'start'
        ? HorizontalAlign.Start
        : HorizontalAlign.Center)
    } else if (node.type === 'Row') {
      Row({ space: this.num(node.props, 'space', 6) }) {
        this.renderChildren(node)
      }
      .width('100%')
      .padding(this.num(node.props, 'padding', 8))
      .justifyContent(FlexAlign.Center)
    } else if (node.type === 'Stack') {
      Stack() {
        this.renderChildren(node)
      }
      .width('100%')
      .padding(this.num(node.props, 'padding', 8))
    } else {
      this.fallback(node)
    }
  }

  build() {
    Column() {
      if (this.node === null) {
        Text('尚无 UI —— 在代码中调用 render(ui.text("Hello")) 试试')
          .fontSize(13)
          .fontColor('#6B7280')
          .margin(12)
      } else {
        this.renderNode(this.node as UiNode)
      }
    }
    .width('100%')
  }
}
