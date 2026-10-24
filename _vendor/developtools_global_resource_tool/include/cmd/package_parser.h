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

#ifndef OHOS_RESTOOL_PACKAGE_PARSER_H
#define OHOS_RESTOOL_PACKAGE_PARSER_H

#include <functional>
#include <getopt.h>
#include <string>
#include <iostream>
#include "resource_data.h"
#include "restool_errors.h"

namespace OHOS {
namespace Global {
namespace Restool {
class PackageParser {
public:
    PackageParser(){};
    virtual ~PackageParser() = default;
    uint32_t Parse(int argc, char *argv[]);
    uint32_t ExecCommand();
    const std::vector<std::string> &GetInputs() const;
    const std::string &GetPackageName() const;
    const std::string &GetOutput() const;
    const std::vector<std::string> &GetResourceHeaders() const;
    bool GetForceWrite() const;
    const std::vector<std::string> &GetModuleNames() const;
    const std::string &GetConfig() const;
    const std::string &GetRestoolPath() const;
    uint32_t GetStartId() const;
    bool IsFileList() const;
    const std::vector<std::string> &GetAppend() const;
    bool GetCombine() const;
    const std::string &GetDependEntry() const;
    const std::string &GetIdDefinedOutput() const;
    const std::string &GetIdDefinedInputPath() const;
    bool GetIconCheck() const;
    const TargetConfig &GetTargetConfigValues() const;
    const std::vector<std::string> &GetSysIdDefinedPaths() const;
    const std::string &GetCompressionPath() const;
    bool IsOverlap() const;
    size_t GetThreadCount() const;

private:
    void InitCommand();
    uint32_t ParseCommand(int argc, char *argv[]);
    uint32_t CheckError(int argc, char *argv[], int c, int optIndex);
    uint32_t AddInput(const std::string &argValue);
    bool IsOverlapInput(const std::string& inputPath);
    uint32_t AddPackageName(const std::string &argValue);
    uint32_t AddOutput(const std::string &argValue);
    uint32_t AddResourceHeader(const std::string &argValue);
    uint32_t ForceWrite();
    uint32_t PrintVersion();
    uint32_t AddMoudleNames(const std::string &argValue);
    uint32_t AddConfig(const std::string &argValue);
    uint32_t AddStartId(const std::string &argValue);
    void AdaptResourcesDirForInput();
    uint32_t CheckParam() const;
    uint32_t HandleProcess(int c, const std::string &argValue);
    uint32_t ParseFileList(const std::string &fileListPath);
    uint32_t AddAppend(const std::string &argValue);
    uint32_t SetCombine();
    uint32_t AddDependEntry(const std::string &argValue);
    uint32_t ShowHelp() const;
    uint32_t SetIdDefinedOutput(const std::string &argValue);
    uint32_t SetIdDefinedInputPath(const std::string &argValue);
    uint32_t AddSysIdDefined(const std::string &argValue);
    bool IsAscii(const std::string &argValue) const;
    bool IsLongOpt(int argc, char *argv[]) const;
    uint32_t IconCheck();
    uint32_t ParseTargetConfig(const std::string &argValue);
    uint32_t AddCompressionPath(const std::string &argValue);
    uint32_t ParseThread(const std::string &argValue);
    uint32_t ParseIgnoreRegex(const std::string &argValue, const std::string &option);

    static const struct option CMD_OPTS[];
    static const std::string CMD_PARAMS;
    using HandleArgValue = std::function<uint32_t(const std::string &)>;
    std::map<int32_t, HandleArgValue> handles_;
    std::vector<std::string> inputs_;
    std::string packageName_;
    std::string output_;
    std::vector<std::string> resourceHeaderPaths_;
    bool forceWrite_ = false;
    std::vector<std::string> moduleNames_;
    std::string configPath_;
    std::string restoolPath_;
    uint32_t startId_ = 0;
    bool isFileList_ = false;
    std::vector<std::string> append_;
    bool combine_ = false;
    std::string dependEntry_;
    std::string idDefinedOutput_;
    std::string idDefinedInputPath_;
    bool isIconCheck_ = false;
    std::vector<std::string> sysIdDefinedPaths_;
    std::string compressionPath_;
    size_t threadCount_{ 0 };
    bool isOverlap_{ false };
};
} // namespace Restool
} // namespace Global
} // namespace OHOS
#endif