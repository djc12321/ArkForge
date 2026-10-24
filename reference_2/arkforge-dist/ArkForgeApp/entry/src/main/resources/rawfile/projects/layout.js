// 工程：布局（示例三）
console.log('工程 layout 已加载');
render(
  ui.column([
    ui.text('工程三：布局', { size: 22, weight: 'bold', color: '#0A59F7' }),
    ui.row([ ui.text('A'), ui.text('B'), ui.text('C'), ui.text('D') ], { space: 10 }),
    ui.row([ ui.text('1'), ui.text('2'), ui.text('3') ], { space: 10 }),
    ui.stack([ ui.text('叠层1'), ui.text('叠层2') ])
  ], { align: 'center', space: 8 })
);
