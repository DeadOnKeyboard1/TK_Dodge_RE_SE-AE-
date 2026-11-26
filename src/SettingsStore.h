#pragma once
#include "SettingsModel.h"
#include <filesystem>

namespace TKDodge::config
{
    inline const std::filesystem::path iniPath{"Data/SKSE/Plugins/TK Dodge RE.ini"};
    bool Load(const std::filesystem::path& path, Values& value, bool allowMissing = false);
    bool Save(const std::filesystem::path& path, const Values& value);
}
