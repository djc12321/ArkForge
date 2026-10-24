// 工程：计数器UI（示例二）
console.log('工程 counter 已加载');
render(
  ui.column([
    ui.text('工程二：计数器UI', { size: 24, weight: 'bold', color: '#B3261E' }),
    ui.text('0', { size: 48, weight: 'bold' }),
    ui.button('+1', {}),
    ui.button('-1', {})
  ], { align: 'center', space: 12 })
);
