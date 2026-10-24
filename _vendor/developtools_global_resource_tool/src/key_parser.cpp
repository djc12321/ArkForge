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

#include "key_parser.h"
#include <regex>
#include "resource_util.h"

namespace OHOS {
namespace Global {
namespace Restool {
using namespace std;
map<string, vector<KeyParam>> KeyParser::caches_ = {};
bool KeyParser::Parse(const string &folderName, vector<KeyParam> &keyparams)
{
    if (folderName == "base") {
        return true;
    }

    if (caches_.count(folderName) != 0) {
        keyparams = caches_[folderName];
        return true;
    }
    vector<string> keyParts;
    ResourceUtil::Split(folderName, keyParts, "-");

    vector<parse_key_founction> founctions = {
        ParseMccMnc,
        ParseLSR,
        ParseOrientation,
        ParseDeviceType,
        ParseNightMode,
        ParseInputDevice,
        ParseResolution,
    };

    if (!ParseMatch(keyParts, keyparams, founctions)) {
        return false;
    }
    caches_.emplace(folderName, keyparams);
    return true;
}

bool KeyParser::ParseMatch(const vector<string> &keys,
    vector<KeyParam> &keyparams, const vector<parse_key_founction> &founctions)
{
    size_t next = 0;
    for (const auto &key : keys) {
        bool parseMatch = false;
        for (size_t i = next; i < founctions.size(); i++) {
            if (founctions[i](key, keyparams)) {
                parseMatch = true;
                next = i;
                break;
            }
        }

        if (!parseMatch) {
            return false;
        }
    }
    return true;
}

bool KeyParser::ParseLimit(const string &func, std::vector<std::string> &limitValues,
    TargetConfig &targetConfig)
{
    map<string, function<bool(const string &)>> parseLimitFunc {
        {"mccmnc", [&targetConfig](const string &limitValue) { return ParseMccMnc(limitValue,
            targetConfig.mccmnc); }},
        {"locale", [&targetConfig](const string &limitValue) { return ParseLSR(limitValue,
            targetConfig.locale); }},
        {"orientation", [&targetConfig](const string &limitValue) { return ParseOrientation(limitValue,
            targetConfig.orientation); }},
        {"device", [&targetConfig](const string &limitValue) { return ParseDeviceType(limitValue,
            targetConfig.device); }},
        {"colormode", [&targetConfig](const string &limitValue) { return ParseNightMode(limitValue,
            targetConfig.colormode); }},
        {"density", [&targetConfig](const string &limitValue) { return ParseResolution(limitValue,
            targetConfig.density); }}
    };

    auto iter = parseLimitFunc.find(func);
    if (iter == parseLimitFunc.end()) {
        return false;
    }
    for (auto &limitValue : limitValues) {
        ResourceUtil::RemoveSpaces(limitValue);
        if (!iter->second(limitValue)) {
            return false;
        }
    }
    return true;
}

bool KeyParser::ParseMatchBySeq(const vector<string> &keys,
    vector<KeyParam> &keyparams, const vector<parse_key_founction> &founctions)
{
    for (size_t i = 0; i < keys.size(); i++) {
        if (!founctions[i](keys[i], keyparams)) {
            return false;
        }
    }
    return true;
}

bool KeyParser::ParseMccMnc(const string &folderName, vector<KeyParam> &keyparams)
{
    vector<parse_key_founction> founctions = {
        ParseMcc,
        ParseMnc,
    };

    vector<string> keyParts;
    ResourceUtil::Split(folderName, keyParts, "_");
    if (keyParts.size() > founctions.size()) {
        return false;
    }
    return ParseMatchBySeq(keyParts, keyparams, founctions);
}

bool KeyParser::ParseMcc(const string &folderName, vector<KeyParam> &keyparams)
{
    regex mcc("mcc(\\d{3})");
    if (!regex_match(folderName, mcc)) {
        return false;
    }

    PushMccMnc(folderName, KeyType::MCC, keyparams);
    return true;
}

bool KeyParser::ParseMnc(const string &folderName, vector<KeyParam> &keyparams)
{
    regex mcc("mnc(\\d{2,3})");
    if (!regex_match(folderName, mcc)) {
        return false;
    }

    PushMccMnc(folderName, KeyType::MNC, keyparams);
    return true;
}

bool KeyParser::ParseLSR(const string &folderName, vector<KeyParam> &keyparams)
{
    map<string, vector<parse_key_founction>> founctionModels = {
        { "all", { ParseLanguage, ParseScript, ParseRegion } },
        { "language script", { ParseLanguage, ParseScript} },
        { "language region", { ParseLanguage, ParseRegion } },
    };

    vector<string> keyParts;
    ResourceUtil::Split(folderName, keyParts, "_");
    if (keyParts.size() > founctionModels["all"].size()) {
        return false;
    }

    for (auto model : founctionModels) {
        vector<KeyParam> tmp;
        if (ParseMatchBySeq(keyParts, tmp, model.second)) {
            keyparams.insert(keyparams.end(), tmp.begin(), tmp.end());
            return true;
        }
    }
    return false;
}

bool KeyParser::ParseLanguage(const string &folderName, vector<KeyParam> &keyparams)
{
    if (g_deviceMap.find(folderName) != g_deviceMap.end()) {
        return false;
    }

    regex language("[a-z]{2,3}");
    if (!regex_match(folderName, language)) {
        return false;
    }

    PushLSR(folderName, KeyType::LANGUAGE, keyparams);
    return true;
}

bool KeyParser::ParseScript(const string &folderName, vector<KeyParam> &keyparams)
{
    if (folderName.length() != SCRIPT_LENGHT) {
        return false;
    }

    regex script("^[A-Z][a-z]{3}");
    if (!regex_match(folderName, script)) {
        return false;
    }

    PushLSR(folderName, KeyType::SCRIPT, keyparams);
    return true;
}

bool KeyParser::ParseRegion(const string &folderName, vector<KeyParam> &keyparams)
{
    if (folderName.length() < MIN_REGION_LENGHT || folderName.length() > MAX_REGION_LENGHT) {
        return false;
    }

    regex regionOfNumber("[0-9]{3}");
    regex regionOfSupper("[A-Z]{2,3}");

    if (!regex_match(folderName, regionOfNumber) && !regex_match(folderName, regionOfSupper)) {
        return false;
    }

    PushLSR(folderName, KeyType::REGION, keyparams);
    return true;
}

bool KeyParser::ParseOrientation(const string &folderName, vector<KeyParam> &keyparams)
{
    auto it = g_orientaionMap.find(folderName);
    if (it == g_orientaionMap.end()) {
        return false;
    }

    PushValue(static_cast<uint32_t>(it->second), KeyType::ORIENTATION, keyparams);
    return true;
}

bool KeyParser::ParseDeviceType(const string &folderName, vector<KeyParam> &keyparams)
{
    auto it = g_deviceMap.find(folderName);
    if (it == g_deviceMap.end()) {
        return false;
    }

    PushValue(static_cast<uint32_t>(it->second), KeyType::DEVICETYPE, keyparams);
    return true;
}

bool KeyParser::ParseNightMode(const string &folderName, vector<KeyParam> &keyparams)
{
    auto it = g_nightModeMap.find(folderName);
    if (it == g_nightModeMap.end()) {
        return false;
    }

    PushValue(static_cast<uint32_t>(it->second), KeyType::NIGHTMODE, keyparams);
    return true;
}

bool KeyParser::ParseInputDevice(const string &folderName, vector<KeyParam> &keyparams)
{
    auto it = g_inputDeviceMap.find(folderName);
    if (it == g_inputDeviceMap.end()) {
        return false;
    }

    PushValue(static_cast<uint32_t>(it->second), KeyType::INPUTDEVICE, keyparams);
    return true;
}

bool KeyParser::ParseResolution(const string &folderName, vector<KeyParam> &keyparams)
{
    auto it = g_resolutionMap.find(folderName);
    if (it == g_resolutionMap.end()) {
        return false;
    }

    PushValue(static_cast<uint32_t>(it->second), KeyType::RESOLUTION, keyparams);
    return true;
}

void KeyParser::PushMccMnc(const string &folderName, KeyType type, vector<KeyParam> &keyparams)
{
    KeyParam keyParam;
    keyParam.keyType = type;
    keyParam.value = static_cast<uint32_t>(atoi(folderName.substr(MCC_MNC_KEY_LENGHT).c_str()));
    keyparams.push_back(keyParam);
}

void KeyParser::PushLSR(const string &folderName, KeyType type, vector<KeyParam> &keyparams)
{
    KeyParam keyParam;
    keyParam.keyType = type;
    keyParam.value = 0;
    for (size_t i = 0; i < folderName.size(); i++) {
        keyParam.value = (keyParam.value << 8) | (folderName[i] & 0xff); // Move 8 bits to the left
    }
    keyparams.push_back(keyParam);
}

void KeyParser::PushValue(uint32_t value, KeyType type, vector<KeyParam> &keyparams)
{
    KeyParam keyParam;
    keyParam.keyType = type;
    keyParam.value = value;
    keyparams.push_back(keyParam);
}
}
}
}
