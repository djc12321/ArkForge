/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifndef OHOS_RESTOOL_KEY_PARSER_H
#define OHOS_RESTOOL_KEY_PARSER_H

#include <vector>
#include "resource_data.h"

namespace OHOS {
namespace Global {
namespace Restool {
class KeyParser {
public:
    static bool Parse(const std::string &folderName, std::vector<KeyParam> &keyparams);
    static bool ParseLimit(const std::string &func, std::vector<std::string> &limitValues,
        TargetConfig &targetConfig);

private:
    typedef bool (*parse_key_founction)(const std::string &folderName, std::vector<KeyParam> &keyparams);
    static bool ParseMatch(const std::vector<std::string> &keys,
        std::vector<KeyParam> &keyparams, const std::vector<parse_key_founction> &founctions);
    static bool ParseMatchBySeq(const std::vector<std::string> &keys,
        std::vector<KeyParam> &keyparams, const std::vector<parse_key_founction> &founctions);

    static bool ParseMccMnc(const std::string &folderName, std::vector<KeyParam> &keyparams);
    static bool ParseMcc(const std::string &folderName, std::vector<KeyParam> &keyparams);
    static bool ParseMnc(const std::string &folderName, std::vector<KeyParam> &keyparams);
    static bool ParseLSR(const std::string &folderName, std::vector<KeyParam> &keyparams);
    static bool ParseLanguage(const std::string &folderName, std::vector<KeyParam> &keyparams);
    static bool ParseScript(const std::string &folderName, std::vector<KeyParam> &keyparams);
    static bool ParseRegion(const std::string &folderName, std::vector<KeyParam> &keyparams);
    static bool ParseOrientation(const std::string &folderName, std::vector<KeyParam> &keyparams);
    static bool ParseDeviceType(const std::string &folderName, std::vector<KeyParam> &keyparams);
    static bool ParseNightMode(const std::string &folderName, std::vector<KeyParam> &keyparams);
    static bool ParseInputDevice(const std::string &folderName, std::vector<KeyParam> &keyparams);
    static bool ParseResolution(const std::string &folderName, std::vector<KeyParam> &keyparams);

    static void PushMccMnc(const std::string &folderName, KeyType type, std::vector<KeyParam> &keyparams);
    static void PushLSR(const std::string &folderName, KeyType type, std::vector<KeyParam> &keyparams);
    static void PushValue(uint32_t value, KeyType type, std::vector<KeyParam> &keyparams);
    static const int32_t MCC_MNC_KEY_LENGHT = 3;
    static const int32_t SCRIPT_LENGHT = 4;
    static const int32_t MIN_REGION_LENGHT = 2;
    static const int32_t MAX_REGION_LENGHT = 3;
    static std::map<std::string, std::vector<KeyParam>> caches_;
};
}
}
}
#endif
