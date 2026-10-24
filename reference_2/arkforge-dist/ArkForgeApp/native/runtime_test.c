/*
 * runtime_test.c —— 引擎逻辑自检
 *
 * 编译为 x86_64 本机可执行，直接调用 vm_eval() 验证核心行为。
 * 目的：交叉编译只能证明"能编过"，跑起来才能证明"逻辑是对的"。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *vm_eval(const char *code);
extern void  vm_reset(void);

static int g_pass = 0;
static int g_fail = 0;

static void check(const char *name, const char *json, const char *expect) {
    int ok = expect ? (strstr(json, expect) != NULL) : 1;
    printf("  [%s] %s\n", ok ? "通过" : "失败", name);
    if (!ok) printf("       实际: %.300s\n", json);
    ok ? g_pass++ : g_fail++;
}

static void run(const char *name, const char *code, const char *expect) {
    printf("\n▸ %s\n", name);
    char *r = vm_eval(code);
    check(name, r, expect);
    printf("  往返耗时数据见 timeMs 字段：%.*s\n", r ? 60 : 0, r ? r : "");
    free(r);
}

int main(void) {
    printf("========================================\n");
    printf(" Harmony Playground · 引擎逻辑自检\n");
    printf("========================================\n");

    run("1. 基本求值", "1 + 2 * 3", "\"value\":\"7\"");

    run("2. console.log 捕获（多参数须合并为一行）",
        "console.log('hello', 42); 'done'",
        "\"logs\":\"hello 42\\n\"");

    run("2b. 多次调用应产生多行",
        "console.log('a'); console.log('b')",
        "\"logs\":\"a\\nb\\n\"");

    run("3. render 输出 UI 树",
        "render(ui.column([ui.text('标题',{size:20})]))",
        "\"type\":\"Column\"");

    run("4. 语法错误捕获", "let x = ;", "\"ok\":false");

    run("5. 抛出异常未被捕获", "throw new Error('boom')", "\"ok\":false");

    printf("\n▸ 6. 死循环保护（关键安全项）\n");
    char *r = vm_eval("while(true){}");
    check("6. 死循环应被超时中止", r, "\"timeout\":true");
    free(r);

    printf("\n▸ 7. 超时后 VM 仍可继续使用（不应崩坏）\n");
    r = vm_eval("40 + 2");
    check("7. 后续执行恢复正常", r, "\"value\":\"42\"");
    free(r);

    printf("\n▸ 8. 重置 VM\n");
    vm_reset();
    r = vm_eval("'after reset'");
    check("8. 重置后仍可用", r, "\"value\":\"\\\"after reset\\\"\"");
    free(r);

    /* ---- 打印第三条用例的完整返回，便于人工核对数据结构 ---- */
    printf("\n========================================\n");
    printf(" 附：render 用例的完整返回值\n");
    printf("========================================\n");
    vm_reset();
    char *demo = vm_eval(
        "console.log('来自 JS');\n"
        "render(ui.column([\n"
        "  ui.text('Hello', {size:22, color:'#0F7B4F'}),\n"
        "  ui.row([ui.text('A'), ui.text('B')], {space:12})\n"
        "], {space:10}));\n"
        "'ok'");
    printf("%s\n", demo);
    free(demo);

    printf("\n========================================\n");
    printf(" 结果：通过 %d 项，失败 %d 项\n", g_pass, g_fail);
    printf("========================================\n");
    return g_fail == 0 ? 0 : 1;
}
