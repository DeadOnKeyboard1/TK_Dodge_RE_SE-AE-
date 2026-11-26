#pragma once

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <string_view>

namespace TKDodge::logic
{
    class TraceBudget
    {
    public:
        explicit TraceBudget(unsigned limit = 32) : remaining(limit) {}
        bool Take()
        {
            auto value = remaining.load(std::memory_order_relaxed);
            while (value != 0) {
                if (remaining.compare_exchange_weak(value, value - 1, std::memory_order_relaxed)) return true;
            }
            return false;
        }
    private:
        std::atomic<unsigned> remaining;
    };

    inline std::string_view Trim(std::string_view value)
    {
        const auto begin = value.find_first_not_of(" \t\r\n");
        if (begin == value.npos) return {};
        return value.substr(begin, value.find_last_not_of(" \t\r\n") - begin + 1);
    }

    inline bool ReadKey(std::string_view value, std::uint32_t& result)
    {
        value = Trim(value);
        if (value.empty()) return false;
        if (value == "-1") { result = UINT32_MAX; return true; }
        std::uint32_t key{};
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), key);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || (key > 281 && key != UINT32_MAX)) return false;
        result = key;
        return true;
    }

    inline bool ReadNonNegative(std::string_view value, float& result)
    {
        value = Trim(value);
        if (value.starts_with('+')) {
            value.remove_prefix(1);
            if (value.starts_with('-') || value.starts_with('+')) return false;
        }
        if (value.empty()) return false;
        float number{};
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !std::isfinite(number)) return false;
        result = number < 0.0f ? 0.0f : number;
        return true;
    }

    inline std::optional<std::string_view> Direction(float x, float y, bool tdm)
    {
        if (!std::isfinite(x) || !std::isfinite(y) || (x == 0 && y == 0)) return std::nullopt;
        if (tdm) return "TKDodgeForward";
        const auto angle = std::atan2(x, y);
        constexpr auto quarter = std::numbers::pi_v<float> / 4;
        if (angle >= -quarter && angle < quarter) return "TKDodgeForward";
        if (angle >= -3 * quarter && angle < -quarter) return "TKDodgeLeft";
        if (angle >= 3 * quarter || angle < -3 * quarter) return "TKDodgeBack";
        return "TKDodgeRight";
    }

    template <class Runtime>
    class ScopedButtonState
    {
    public:
        explicit ScopedButtonState(Runtime& value) : target(value), saved(value) {}
        ~ScopedButtonState() { target = saved; }
        ScopedButtonState(const ScopedButtonState&) = delete;
        ScopedButtonState& operator=(const ScopedButtonState&) = delete;
    private:
        Runtime& target;
        Runtime saved;
    };

    class SprintTap
    {
    public:
        enum class Action { pass, consume, dodge, startSprint };
        void Reset() { phase = Phase::idle; key = 0; }
        Action Process(std::uint64_t input, bool down, bool up, float held, bool sprinting, float threshold)
        {
            if (!std::isfinite(held) || held < 0 || !std::isfinite(threshold) || threshold < 0) {
                Reset();
                return Action::pass;
            }
            if (phase != Phase::idle && input != key) {
                if (!down) return Action::consume;
                Reset();
            }
            if (down) {
                key = input;
                phase = sprinting || threshold == 0 ? Phase::native : Phase::pending;
                if (sprinting) return Action::pass;
                return threshold == 0 ? Action::startSprint : Action::consume;
            }
            if (phase == Phase::idle) return Action::pass;
            if (up) {
                const auto pending = phase == Phase::pending;
                Reset();
                return pending && held < threshold ? Action::dodge : Action::pass;
            }
            if (phase == Phase::pending) {
                if (held < threshold) return Action::consume;
                phase = Phase::native;
                return Action::startSprint;
            }
            return Action::pass;
        }
    private:
        enum class Phase { idle, pending, native };
        Phase phase{Phase::idle};
        std::uint64_t key{};
    };

    class SneakTap
    {
    public:
        enum class Action { pass, consume, dodge, press };
        void Reset() { phase = Phase::idle; key = 0; }
        Action Process(std::uint64_t input, bool down, bool up, float held, float threshold)
        {
            if (!std::isfinite(held) || held < 0 || !std::isfinite(threshold) || threshold < 0) {
                Reset();
                return Action::pass;
            }
            if (phase != Phase::idle && input != key) {
                // Recover a lost key-up on a fresh press, never on a stray release.
                if (!down) return Action::consume;
                Reset();
            }
            if (down) {
                key = input;
                phase = threshold == 0 ? Phase::committed : Phase::pending;
                return threshold == 0 ? Action::press : Action::consume;
            }
            if (phase == Phase::idle) return Action::pass;
            if (up) {
                const auto pending = phase == Phase::pending;
                Reset();
                if (!pending) return Action::pass;
                return held < threshold ? Action::dodge : Action::press;
            }
            if (phase == Phase::pending) {
                if (held < threshold) return Action::consume;
                phase = Phase::committed;
                return Action::press;
            }
            return Action::pass;
        }
    private:
        enum class Phase { idle, pending, committed };
        Phase phase{Phase::idle};
        std::uint64_t key{};
    };
}
