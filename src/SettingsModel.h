#pragma once

#include <cmath>
#include <cstdint>
#include <string>

namespace TKDodge::config
{
    struct Values
    {
        std::uint32_t dodgeKey = 42;
        bool EnableSprintKeyDodge = false;
        bool EnableSneakKeyDodge = false;
        bool stepDodge = false;
        float dodgeStamina = 10.0f;
        bool enableSneakDodge = false;
        bool enableDodgeAttackCancel = true;
        float iFrameDuration = 0.3f;
        std::string defaultDodgeEvent = "TKDodgeBack";
        float SprintingPressDuration = 0.3f;
        float SneakingPressDuration = 0.3f;
        bool debugLog = false;
        bool operator==(const Values&) const = default;
    };

    inline bool Valid(const Values& value)
    {
        const auto nonnegative = [](float number) { return std::isfinite(number) && number >= 0; };
        return (value.dodgeKey <= 281 || value.dodgeKey == UINT32_MAX) &&
               nonnegative(value.dodgeStamina) && nonnegative(value.iFrameDuration) &&
               nonnegative(value.SprintingPressDuration) && nonnegative(value.SneakingPressDuration);
    }
}
