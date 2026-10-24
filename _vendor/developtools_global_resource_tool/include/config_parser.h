/*
 * Copyright (c) 2021-2025 Huawei Device Co., Ltd.
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

#ifndef OHOS_RESTOOL_CONFIG_PARSER_H
#define OHOS_RESTOOL_CONFIG_PARSER_H

#include <functional>
#include <set>
#include <cJSON.h>
#include "resource_util.h"

namespace OHOS {
namespace Global {
namespace Restool {
class ConfigParser {
public:
    enum class ModuleType {
        NONE = 0,
        HAR = 1,
        ENTRY = 2,
        FEATURE = 3,
        SHARED = 4,
    };

    ConfigParser();
    explicit ConfigParser(const std::string &filePath);
    virtual ~ConfigParser();
    uint32_t Init();
    const std::string &GetPackageName() const;
    const std::string &GetModuleName() const;
    int64_t GetAbilityIconId() const;
    int64_t GetAbilityLabelId() const;
    bool SetAppIcon(std::string &icon, int64_t id);
    bool SetAppLabel(std::string &label, int64_t id);
    ModuleType GetModuleType() const;
    uint32_t ParseRefence();
    uint32_t Save(const std::string &filePath) const;
    const std::map<std::string, std::set<uint32_t>> GetCheckNode() const
    {
        return jsonCheckIds_;
    }
    void SetDependEntry(const bool isDenpend)
    {
        dependEntry = isDenpend;
    }
    bool IsDependEntry()
    {
        return dependEntry;
    }
    bool IsHar()
    {
        return moduleType_ == ModuleType::HAR;
    }
    std::string GetConfigFilePath()
    {
        return filePath_;
    }
    inline bool isSupportNewModule()
    {
        return newModule_;
    }
    inline bool isSupportTsHeader() const
    {
        return tsHeader_;
    }
    static void SetUseModule()
    {
        useModule_ = true;
    };
    static std::string GetConfigName()
    {
        return useModule_ ? MODULE_JSON : CONFIG_JSON;
    };
private:
    bool ParseModule(cJSON *moduleNode);
    bool ParseDistro(cJSON *distroNode);
    bool ParseAbilities(const cJSON *abilities);
    bool ParseAbilitiy(const cJSON *ability, bool &isMainAbility);
    bool IsMainAbility(const cJSON *skills);
    bool IsHomeAction(const cJSON *actions);
    bool dependEntry = false;
    bool ParseRefImpl(cJSON *parent, const std::string &key, cJSON *node);
    bool ParseJsonArrayRef(cJSON *parent, const std::string &key, cJSON *node);
    bool ParseJsonStringRef(cJSON *parent, const std::string &key, cJSON *node);
    bool GetRefIdFromString(std::string &value, bool &update, const std::string &match) const;
    bool ParseModuleType(const std::string &type);
    bool ParseAbilitiesForDepend(cJSON *moduleNode);
    void AddCheckNode(const std::string &key, uint32_t id);
    std::string filePath_;
    std::string packageName_;
    std::string moduleName_;
    ModuleType moduleType_;
    std::string mainAbility_;
    int64_t abilityIconId_;
    int64_t abilityLabelId_;
    std::map<std::string, std::set<uint32_t>> jsonCheckIds_;
    static const std::map<std::string, ModuleType> MODULE_TYPES;
    static const std::map<std::string, std::string> JSON_STRING_IDS;
    static const std::map<std::string, std::string> JSON_ARRAY_IDS;
    static bool useModule_;
    cJSON *root_;
    bool newModule_ = false;
    bool tsHeader_ = false;
};
}
}
}
#endif