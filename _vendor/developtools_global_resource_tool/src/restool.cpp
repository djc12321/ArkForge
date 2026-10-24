/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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
#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
using namespace OHOS::Global::Restool;

int main(int argc, char *argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    if (argv == nullptr) {
        PrintError(GetError(ERR_CODE_UNKNOWN_COMMAND_ERROR).FormatCause("argv null"));
        return RESTOOL_ERROR;
    }
    try {
        InitFaq(std::string(argv[0]));
    } catch (const std::runtime_error &error) {
        return RESTOOL_ERROR;
    }
    auto &parser = CmdParser::GetInstance();
    return parser.Parse(argc, argv, 1);
}
