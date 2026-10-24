/*
 * Copyright (c) 2024-2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cmd/dump_parser.h"
#include <memory>
#include <string>
#include "cmd/cmd_parser.h"
#include "resource_util.h"
#include "resource_dumper.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
uint32_t DumpParserBase::ParseOption(int argc, char *argv[], int currentIndex)
{
    if (currentIndex >= argc || currentIndex < 0) {
        PrintError(ERR_CODE_DUMP_MISSING_INPUT);
        return RESTOOL_ERROR;
    }
    inputPath_ = ResourceUtil::RealPath(argv[currentIndex]);
    if (inputPath_.empty()) {
        PrintError(GetError(ERR_CODE_DUMP_INVALID_INPUT).FormatCause(argv[currentIndex]));
        return RESTOOL_ERROR;
    }
    return RESTOOL_SUCCESS;
}

const string& DumpParserBase::GetInputPath() const
{
    return inputPath_;
}

DumpParser::DumpParser() : CmdParserBase("dump")
{
    subcommands_.emplace_back(std::make_unique<DumpConfigParser>());
}

uint32_t DumpParser::ExecCommand()
{
    return CommonDumper().Dump(*this);
}

void DumpParserBase::ShowUseage()
{
    std::cout << "Usage:\n";
    std::cout << "restool dump [subcommand] [options] file.\n";
    std::cout << "[subcommands]:\n";
    std::cout << "    config                Print config of the resource in the hap.\n";
    std::cout << "\n";
    std::cout << "[options]:\n";
    std::cout << "    -h                    Print dump subcommand help info.\n";
}

DumpConfigParser::DumpConfigParser() : CmdParserBase("config")
{}

uint32_t DumpConfigParser::ExecCommand()
{
    return ConfigDumper().Dump(*this);
}
} // namespace Restool
} // namespace Global
} // namespace OHOS