#include "cmd/cmd_parser.h"
#include "thread_pool.h"
#include <exception>
#include <cstdio>
#include <getopt.h>

// Each invocation is dlopened separately. Stop all owned workers before dlclose
// so upstream singleton configuration cannot leak into a later build.
extern "C" __attribute__((visibility("default"))) int ArkForgeResourceRun(int argc, char **argv)
{
    using namespace OHOS::Global::Restool;
    int status = -1;
    optind = 0; optarg = nullptr; optopt = 0;
    try { status = CmdParser::GetInstance().Parse(argc, argv, 1); }
    catch (const std::exception &error) { fprintf(stderr, "restool: %s\n", error.what()); }
    catch (...) { fprintf(stderr, "restool: unknown compiler exception\n"); }
    ThreadPool::GetInstance().Stop();
    return status;
}
