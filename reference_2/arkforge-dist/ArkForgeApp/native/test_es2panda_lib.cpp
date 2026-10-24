/*
 * test_es2panda_lib.cpp —— 终极验证
 *
 * 直接链接官方 es2panda-lib 动态库，把 TypeScript 源码编译成方舟字节码(.abc)。
 * 这个程序证明：ArkTS 编译器可以作为一个「库函数」被调用，
 * 不需要 fork / exec 任何进程 —— 这正是绕开鸿蒙三方应用限制的核心手段。
 */

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>

#include <es2panda.h>
#include <util/options.h>
#include <util/diagnosticEngine.h>
#include <util/eheap.h>

#include <assembly-emitter.h>
#include <assembly-program.h>
#include <file_writer.h>

using ark::es2panda::Compiler;
using ark::es2panda::ScriptExtension;
using ark::es2panda::SourceFile;
using ark::pandasm::Program;
using ark::pandasm::AsmEmitter;

namespace {

std::string ReadFileOrUse(const char *path, const std::string &fallback)
{
    std::ifstream in(path);
    if (in.good()) {
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
    return fallback;
}

}  // namespace

int main(int argc, char **argv)
{
    const char *inputPath = argc > 1 ? argv[1] : nullptr;
    const char *outputPath = argc > 2 ? argv[2] : "out.abc";

    const std::string defaultSource =
        "function add(a: number, b: number): number {\n"
        "    return a + b;\n"
        "}\n"
        "function greet(name: string): string {\n"
        "    return \"Hello, \" + name;\n"
        "}\n";

    const std::string source = inputPath ? ReadFileOrUse(inputPath, defaultSource) : defaultSource;

    setvbuf(stdout, nullptr, _IONBF, 0);

    printf("=== es2panda-lib 库化调用测试 ===\n");
    printf("输入源码 %zu 字节，输出 %s\n\n", source.length(), outputPath);

    /* 0. 官方 main() 的第一步：初始化内存池。
     *    这是 Etapa/方舟编译器所有 ArenaAllocator 的根，缺了它必崩。 */
    ark::es2panda::ScopedAllocatorsManager::Initialize();
    printf("[init] 内存池已初始化\n");

    /* 1. 构造编译输入 */
    SourceFile sourceFile(inputPath ? inputPath : "inline.ts", source, /* isModule */ true);

    /* 2 & 3. 诊断引擎 + 编译选项
     *        Options 需要「可执行文件路径」来定位内置根目录，
     *        并以引用方式持有 DiagnosticEngine */
    ark::es2panda::util::DiagnosticEngine diagnosticEngine;
    std::string execPath = argv[0] != nullptr ? argv[0] : ".";
    ark::es2panda::util::Options options(execPath, diagnosticEngine);

    /*    走官方的参数解析流程（等价于命令行 es2abc <输入> --output <输出>） */
    const char *cliArgs[] = {execPath.c_str(), inputPath ? inputPath : "inline.ts", "--output", outputPath};
    if (!options.Parse(ark::Span<const char *const>(cliArgs, 4))) {
        printf("[FAIL] 选项解析失败（等价于命令行参数不合法）\n");
        diagnosticEngine.FlushDiagnostic();
        return 1;
    }
    printf("[init] 选项解析通过\n");

    /*
     * 4. 关键调用 —— 与 es2abc 可执行程序内部走的是同一个类。
     *    区别只在：可执行文件先 fork 出一个进程再走到这，
     *    而我们是在自己的进程里直接调函数。
     */
    Compiler compiler(ScriptExtension::TS);

    std::unordered_map<std::string, std::unique_ptr<Program>> programs =
        compiler.Compile(sourceFile, options, diagnosticEngine);

    if (programs.empty()) {
        printf("[FAIL] 编译未产出任何字节码\n");
        printf("       错误: %s\n", compiler.GetError().Message().c_str());
        return 1;
    }

    printf("[OK] 编译成功，产出 %zu 个 Program\n", programs.size());

    /* 5. 发射字节码到文件 */
    for (auto &[name, program] : programs) {
        printf("     program: %s\n", name.c_str());

        /* 用官方 FileWriter 直接落到磁盘，绕过手工序列化 */
        ark::panda_file::FileWriter writer(outputPath);
        if (!AsmEmitter::Emit(&writer, *program)) {
            printf("[FAIL] 字节码发射失败: %s\n", name.c_str());
            return 1;
        }
        printf("[OK] 已写出字节码到 %s\n", outputPath);
        break;
    }

    printf("\n=== 结论：方舟编译器已在库模式下完整跑通 ===\n");
    return 0;
}
