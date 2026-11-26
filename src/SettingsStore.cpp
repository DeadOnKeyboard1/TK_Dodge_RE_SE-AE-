#include "SettingsStore.h"
#include "DodgeLogic.h"
#include <SimpleIni.h>
#include <Windows.h>
#include <atomic>
#include <limits>

namespace TKDodge::config
{
    namespace
    {
        bool ReadIni(const std::filesystem::path& path, CSimpleIniA& ini, bool allowMissing)
        {
            std::error_code error;
            const auto exists = std::filesystem::exists(path, error);
            if (error) return false;
            if (!exists) return allowMissing;
            return ini.LoadFile(path.c_str()) >= 0;
        }

        bool AtomicWrite(const std::filesystem::path& path, const std::string& text)
        {
            if (text.size() > (std::numeric_limits<DWORD>::max)()) return false;
            static std::atomic<unsigned> counter{};
            auto temporary = path;
            temporary += ".tkre-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(counter++) + ".tmp";
            const auto file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE) return false;
            DWORD written{};
            const bool complete = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
                                  written == text.size() && FlushFileBuffers(file);
            CloseHandle(file);
            if (complete && MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
            DeleteFileW(temporary.c_str());
            return false;
        }
    }

    bool Load(const std::filesystem::path& path, Values& value, bool allowMissing)
    {
        CSimpleIniA ini;
        ini.SetUnicode();
        if (!ReadIni(path, ini, allowMissing)) return false;
        auto result = value;
        const auto readBool = [&](const char* key, bool& field) { field = ini.GetBoolValue("Main", key, field); };
        const auto readFloat = [&](const char* key, float& field) {
            if (const auto text = ini.GetValue("Main", key)) logic::ReadNonNegative(text, field);
        };
        if (const auto text = ini.GetValue("Main", "DodgeHotkey")) logic::ReadKey(text, result.dodgeKey);
        readBool("EnableSprintKeyDodge", result.EnableSprintKeyDodge);
        readBool("EnableSneakKeyDodge", result.EnableSneakKeyDodge);
        readBool("StepDodge", result.stepDodge);
        readBool("EnableSneakDodge", result.enableSneakDodge);
        readBool("EnableDodgeAttackCancel", result.enableDodgeAttackCancel);
        readBool("EnableDebugLog", result.debugLog);
        readFloat("DodgeStamina", result.dodgeStamina);
        readFloat("iFrameDuration", result.iFrameDuration);
        readFloat("SprintingPressDuration", result.SprintingPressDuration);
        readFloat("SneakingPressDuration", result.SneakingPressDuration);
        result.defaultDodgeEvent = ini.GetValue("Main", "defaultDodgeEvent", result.defaultDodgeEvent.c_str());
        if (!Valid(result)) return false;
        value = std::move(result);
        return true;
    }

    bool Save(const std::filesystem::path& path, const Values& value)
    {
        if (!Valid(value)) return false;
        CSimpleIniA ini;
        ini.SetUnicode();
        if (!ReadIni(path, ini, true)) return false;
        bool success = true;
        const auto set = [&](const char* key, const std::string& text) { success &= ini.SetValue("Main", key, text.c_str()) >= 0; };
        const auto boolean = [&](const char* key, bool field) { set(key, field ? "true" : "false"); };
        const auto number = [&](const char* key, float field) {
            char buffer[64];
            const auto result = std::to_chars(buffer, buffer + sizeof(buffer), field, std::chars_format::general, std::numeric_limits<float>::max_digits10);
            if (result.ec != std::errc{}) { success = false; return; }
            set(key, std::string(buffer, result.ptr));
        };
        set("DodgeHotkey", value.dodgeKey == UINT32_MAX ? "-1" : std::to_string(value.dodgeKey));
        boolean("EnableSprintKeyDodge", value.EnableSprintKeyDodge);
        boolean("EnableSneakKeyDodge", value.EnableSneakKeyDodge);
        boolean("StepDodge", value.stepDodge);
        boolean("EnableSneakDodge", value.enableSneakDodge);
        boolean("EnableDodgeAttackCancel", value.enableDodgeAttackCancel);
        boolean("EnableDebugLog", value.debugLog);
        number("DodgeStamina", value.dodgeStamina);
        number("iFrameDuration", value.iFrameDuration);
        number("SprintingPressDuration", value.SprintingPressDuration);
        number("SneakingPressDuration", value.SneakingPressDuration);
        set("defaultDodgeEvent", value.defaultDodgeEvent);
        std::string text;
        return success && ini.Save(text) >= 0 && AtomicWrite(path, text);
    }
}
