/**
 * UiNode.ets —— 动态 UI 描述协议
 *
 * Native 层的 JS 引擎通过 __host.render(json) 吐出这棵树，
 * ArkTS 侧递归渲染成真实的 ArkUI 组件。
 *
 * 设计：刻意保持为一个极小的受控子集（容器 + 少数基础组件），
 *       因为这是解释执行，不支持任意 ArkTS 语法，只支持描述的节点组合。
 */
export interface UiProps {
  size?: number;
  color?: string;
  weight?: string;
  margin?: number;
  placeholder?: string;
  src?: string;
  width?: number;
  height?: number;
  space?: number;
  padding?: number;
  align?: string;
}

export interface UiNode {
  type: string;
  value?: string;
  props?: UiProps;
  children?: UiNode[];
}

/** 支持的组件类型白名单 —— 渲染器只认这些，其余降级为文本提示 */
export const SUPPORTED_TYPES: string[] = [
  'Text', 'Button', 'Input', 'Image', 'Column', 'Row', 'Stack'
];

export function isContainer(node: UiNode): boolean {
  return node.type === 'Column' || node.type === 'Row' || node.type === 'Stack';
}
