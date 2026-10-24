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


#include "cmd/cmd_parser.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include "cmd/dump_parser.h"
#include "cmd/package_parser.h"
#include "restool_errors.h"

namespace OHOS {
namespace Global {
namespace Restool {
CmdParserBase::CmdParserBase(const std::string &name) : name_(name)
{}

uint32_t CmdParserBase::Parse(int argc, char *argv[], int currentIndex)
{
    if (currentIndex < argc) {
        for (const auto &subcommand : subcommands_) {
            if (subcommand->name_ == argv[currentIndex]) {
                return subcommand->Parse(argc, argv, currentIndex + 1);
            };
        }
        if (std::strcmp(argv[currentIndex], "-h") == 0) {
            ShowUseage();
            return RESTOOL_SUCCESS;
        }
    }
    uint32_t errorCode = ParseOption(argc, argv, currentIndex);
    if (errorCode != RESTOOL_SUCCESS) {
        if (showUseage_) {
            ShowUseage();
        }
        return errorCode;
    }
    return ExecCommand();
}

uint32_t CmdParserBase::ParseOption(int argc, char *argv[], int currentIndex)
{
    return RESTOOL_SUCCESS;
}

CmdParser::CmdParser() : CmdParserBase("")
{
    subcommands_.emplace_back(std::make_unique<DumpParser>());
}

uint32_t CmdParser::ParseOption(int argc, char *argv[], int currentIndex)
{
    if (currentIndex >= argc) {
        return RESTOOL_ERROR;
    }
    return packageParser_.Parse(argc, argv);
}

uint32_t CmdParser::ExecCommand()
{
    return packageParser_.ExecCommand();
}

PackageParser &CmdParser::GetPackageParser()
{
    return packageParser_;
}

void CmdParser::ShowUseage()
{
    std::cout << "This is an OHOS Packaging Tool.\n";
    std::cout << "Usage:\n";
    std::cout << TOOL_NAME << " [subcommand] files...\n";
    std::cout << TOOL_NAME << " [option1] [option2] [options] files...\n";
    std::cout << "[subcommands]:\n";
    std::cout << "    dump                Print the contents of the resource in the hap."
        "For details about the usage of dump, see '-h'.\n";
    std::cout << "\n";
    std::cout << "[options]:\n";
    std::cout << "    -i/--inputPath      Input resource path, can add multiple.\n";
    std::cout << "    -p/--packageName    Package name.\n";
    std::cout << "    -o/--outputPath     Output path.\n";
    std::cout << "    -r/--resHeader      Resource header file path(like ./ResourceTable.js, ./ResrouceTable.h).\n";
    std::cout << "    -f/--forceWrite     If output path exists,force delete it.\n";
    std::cout << "    -v/--version        Print tool version.\n";
    std::cout << "    -m/--modules        Module names, can add multiple, split by ','(like entry1,entry2,...).\n";
    std::cout << "    -j/--json           Path of module.json(in Stage model) or onfig.json(in FA model).\n";
    std::cout << "    -e/--startId        Start id mask, e.g 0x01000000,";
    std::cout << " in the scope [0x01000000, 0x06FFFFFF),[0x08000000, 0xFFFFFFFF).\n";
    std::cout << "    -x/--append         Resources folder path.\n";
    std::cout << "    -z/--combine        Flag for incremental compilation.\n";
    std::cout << "    -h/--help           Displays this help menu.\n";
    std::cout << "    -l/--fileList       Input json file of the option set, e.g resConfig.json.";
    std::cout << " For details, see the developer documentation.\n";
    std::cout << "    --ids               Save id_defined.json direcory.\n";
    std::cout << "    --defined-ids       Input id_defined.json path.\n";
    std::cout << "    --dependEntry       Build result directory of the specified entry module when the feature";
    std::cout << " module resources are independently built in the FA model.\n";
    std::cout << "    --icon-check        Enable the PNG image verification function for icons and startwindows.\n";
    std::cout << "    --target-config     When used with '-i', selective compilation is supported.\n";
    std::cout << "    --compressed-config Path of opt-compression.json.\n";
    std::cout << "    --thread            Subthreads count.\n";
    std::cout << "    --ignored-file      Regular patterns of ignored files, split by ':'(like \\.git:\\.svn).\n";
    std::cout << "    --ignored-path      Regular patterns of ignored file paths, split by ':'";
    std::cout << "(like .+/rawfile/\\.git:.+/resfile/\\.svn).\n";
}
}
}
}