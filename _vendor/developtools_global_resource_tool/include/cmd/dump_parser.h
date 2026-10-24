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

#ifndef OHOS_RESTOOL_DUMP_PARSER_H
#define OHOS_RESTOOL_DUMP_PARSER_H

#include <memory>
#include <string>
#include "cmd_parser.h"

namespace OHOS {
namespace Global {
namespace Restool {
class DumpParserBase : public virtual CmdParserBase {
public:
    virtual ~DumpParserBase() = default;
    uint32_t ParseOption(int argc, char *argv[], int currentIndex) override;
    void ShowUseage() override;
    const std::string &GetInputPath() const;

protected:
    std::string inputPath_;
};

class DumpParser : public virtual DumpParserBase {
public:
    DumpParser();
    uint32_t ExecCommand() override;
};

class DumpConfigParser : public virtual DumpParserBase {
public:
    DumpConfigParser();
    virtual ~DumpConfigParser() = default;
    uint32_t ExecCommand() override;
};
} // namespace Restool
} // namespace Global
} // namespace OHOS
#endif
