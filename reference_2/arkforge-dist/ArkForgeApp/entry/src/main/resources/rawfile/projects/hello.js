// 工程：Hello（示例一）
console.log('工程 hello 已加载');
let s = 0;
for (let i = 1; i <= 10; i++) s += i;
render(
  ui.column([
    ui.text('工程一：Hello', { size: 24, weight: 'bold', color: '#0F7B4F' }),
    ui.text('1..10 求和 = ' + s, { size: 16 }),
    ui.row([ ui.text('左'), ui.text('中'), ui.text('右') ], { space: 12 })
  ], { align: 'center', space: 10 })
);
s;
