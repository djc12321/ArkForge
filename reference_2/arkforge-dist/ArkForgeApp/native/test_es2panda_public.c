/*
 * test_es2panda_public.c —— 终极验证（官方公开 C API 版）
 *
 * 用官方为「嵌入场景」设计的 C API 编译 TypeScript 出方舟字节码。
 * 相比 C++ API，这个接口才是给 NAPI 用的正确形态：
 *   - 纯 C，extern "C"，无 C++ ABI 问题
 *   - 自带 MemInitialize / MemFinalize，不用自己管内存池
 *   - CreateConfig(argc, argv) 直接吃 es2abc 的命令行参数
 *   - CreateContextFromString 可以直接吃内存里的源码字符串
 *
 * 这个程序跑通 = 「在进程内调用方舟编译器」这一步被彻底证实。
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <es2panda_lib.h>

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc < 4) {
        fprintf(stderr, "Usage: %s INPUT.ets OUTPUT.abc EXEC_PATH [--source]\n", argv[0]);
        return 2;
    }
    const char *inputPath = argv[1];
    const char *outputPath = argv[2];
    if (strcmp(inputPath, outputPath) == 0) {
        fprintf(stderr, "Input and output must be different files\n");
        return 2;
    }
    /* A failed compilation must never leave a previous artifact looking fresh. */
    remove(outputPath);

    printf("=== es2panda 公开 C API 库化调用测试 ===\n");
    printf("输入: %s\n输出: %s\n\n", inputPath, outputPath);

    /* 1. 取官方实现（版本协商） */
    const struct es2panda_Impl *impl = es2panda_GetImpl(ES2PANDA_LIB_VERSION);
    if (impl == NULL) {
        printf("[FAIL] 无法获取 es2panda 实现（版本不匹配？）\n");
        return 1;
    }
    printf("[1/5] 取得实现，API 版本=%d，库版本=%d\n", ES2PANDA_LIB_VERSION, impl->version);

    /* 2. 初始化内存子系统（官方要求的第一步） */
    impl->MemInitialize();
    printf("[2/5] 内存子系统已初始化\n");

    /* 3. 用 es2abc 的命令行参数构造配置
     *    注意 argv[0] 会被引擎用来定位内置资源目录（arktsconfig.json、etsstdlib 等），
     *    所以这里必须传真实的 es2abc 所在路径，不能随便写 "es2abc"。 */
    const char *execPath = argv[3];
    const char *cliArgs[] = {execPath, inputPath, "--output", outputPath};
    es2panda_Config *config = impl->CreateConfig(4, cliArgs);
    if (config == NULL) {
        printf("[FAIL] CreateConfig 失败\n");
        impl->MemFinalize();
        return 1;
    }
    printf("[3/5] 配置已创建（参数：es2abc %s --output %s）\n", inputPath, outputPath);

    /* 4. 从源文件创建编译上下文 */
    es2panda_Context *context = NULL;
    if (argc > 4 && strcmp(argv[4], "--source") == 0) {
        FILE *sourceFile = fopen(inputPath, "rb");
        if (sourceFile == NULL || fseek(sourceFile, 0, SEEK_END) != 0) {
            if (sourceFile) fclose(sourceFile);
            impl->DestroyConfig(config);
            impl->MemFinalize();
            return 1;
        }
        long size = ftell(sourceFile);
        char *source = size >= 0 ? malloc((size_t)size + 1) : NULL;
        rewind(sourceFile);
        if (source != NULL && fread(source, 1, (size_t)size, sourceFile) == (size_t)size) {
            source[size] = '\0';
            context = impl->CreateContextFromString(config, source, inputPath);
        }
        free(source);
        fclose(sourceFile);
    } else {
        context = impl->CreateContextFromFile(config, inputPath);
    }
    if (context == NULL) {
        printf("[FAIL] CreateContextFromFile 失败\n");
        impl->DestroyConfig(config);
        impl->MemFinalize();
        return 1;
    }
    printf("[4/5] 编译上下文已创建，初始状态=%d\n", impl->ContextState(context));

    /* 5. 一路推进到「字节码已生成」 */
    context = impl->ProceedToState(context, ES2PANDA_STATE_BIN_GENERATED);
    if (context == NULL) {
        fprintf(stderr, "[FAIL] ProceedToState returned NULL\n");
        impl->DestroyConfig(config);
        impl->MemFinalize();
        return 1;
    }

    es2panda_ContextState state = impl->ContextState(context);
    printf("[5/5] 推进完成，最终状态=%d（BIN_GENERATED=%d）\n", state, ES2PANDA_STATE_BIN_GENERATED);

    if (impl->IsAnyError(context) || state != ES2PANDA_STATE_BIN_GENERATED) {
        printf("\n[FAIL] 编译出错\n");
        char const *msg = impl->ContextErrorMessage(context);
        if (msg != NULL) {
            printf("       %s\n", msg);
        }
        impl->DestroyContext(context);
        impl->DestroyConfig(config);
        impl->MemFinalize();
        return 1;
    }

    FILE *output = fopen(outputPath, "rb");
    unsigned char magic[8] = {0};
    int valid = output != NULL && fread(magic, 1, sizeof(magic), output) == sizeof(magic)
        && memcmp(magic, "PANDA\0\0\0", sizeof(magic)) == 0;
    if (output) fclose(output);
    printf(valid ? "\n=== 成功：方舟编译器已在进程内完成编译，PANDA 魔数有效 ===\n"
                 : "\n[FAIL] Missing output or invalid PANDA magic\n");

    impl->DestroyContext(context);
    impl->DestroyConfig(config);
    impl->MemFinalize();
    return valid ? 0 : 1;
}
