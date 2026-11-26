#include "DodgeLogic.h"
#include "SettingsStore.h"
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <type_traits>

static unsigned checks{};
#define CHECK(x) do { ++checks; if (!(x)) { std::cerr << "FAILED line " << __LINE__ << ": " #x "\n"; return 1; } } while (false)

int main()
{
    using namespace TKDodge::logic;
    TraceBudget trace(2), empty(0);
    CHECK(trace.Take());
    CHECK(trace.Take());
    CHECK(!trace.Take());
    CHECK(!trace.Take());
    CHECK(!empty.Take());
    std::uint32_t key = 42;
    for (auto value : {"", "abc", "nan", "42.5", "-2", "282", "9999999999999999999", "42junk"}) {
        CHECK(!ReadKey(value, key));
        CHECK(key == 42);
    }
    CHECK(ReadKey(" -1 ", key) && key == UINT32_MAX);
    CHECK(ReadKey("4294967295", key) && key == UINT32_MAX);
    CHECK(ReadKey("281", key) && key == 281);
    CHECK(ReadKey("0", key) && key == 0);
    float value = 0.3f;
    for (auto text : {"", "nan", "inf", "1e99", "3junk", "+-1"}) {
        CHECK(!ReadNonNegative(text, value));
        CHECK(value == 0.3f);
    }
    CHECK(ReadNonNegative("-3", value) && value == 0);
    CHECK(ReadNonNegative(" +0.2 ", value) && value == 0.2f);
    CHECK(!Direction(0, 0, false));
    CHECK(!Direction(NAN, 1, false));
    CHECK(!Direction(1, INFINITY, false));
    CHECK(Direction(0, 1, false) == "TKDodgeForward");
    CHECK(Direction(-1, 0, false) == "TKDodgeLeft");
    CHECK(Direction(1, 0, false) == "TKDodgeRight");
    CHECK(Direction(0, -1, false) == "TKDodgeBack");
    CHECK(Direction(-1, 0, true) == "TKDodgeForward");
    CHECK(!Direction(0, 0, true));
    for (int x = -10; x <= 10; ++x) for (int y = -10; y <= 10; ++y) {
        if (!x && !y) continue;
        const auto length = std::sqrt(float(x*x+y*y));
        const auto angle = std::atan2(float(x)/length, float(y)/length);
        constexpr float pi8 = 0.39269908f;
        std::string_view old = angle >= -2*pi8 && angle < 2*pi8 ? "TKDodgeForward" :
            angle >= -6*pi8 && angle < -2*pi8 ? "TKDodgeLeft" :
            angle >= 6*pi8 || angle < -6*pi8 ? "TKDodgeBack" : "TKDodgeRight";
        CHECK(Direction(float(x), float(y), false) == old);
    }
    constexpr std::uint64_t keyboard = 29;
    constexpr std::uint64_t gamepad = (std::uint64_t{2} << 32) | 64;
    SprintTap tap;
    using A = SprintTap::Action;
    CHECK(tap.Process(keyboard, false, true, 0.1f, false, 0.3f) == A::pass);
    CHECK(tap.Process(keyboard, false, false, 0.4f, false, 0.3f) == A::pass);
    CHECK(tap.Process(keyboard, true, false, 0, false, 0.3f) == A::consume);
    CHECK(tap.Process(keyboard, false, true, 0.1f, false, 0.3f) == A::dodge);
    CHECK(tap.Process(keyboard, true, false, 0, true, 0.3f) == A::pass);
    CHECK(tap.Process(keyboard, false, false, 0.05f, false, 0.3f) == A::pass);
    CHECK(tap.Process(keyboard, false, true, 0.1f, false, 0.3f) == A::pass);
    CHECK(tap.Process(keyboard, true, false, 0, false, 0.3f) == A::consume);
    CHECK(tap.Process(keyboard, false, false, 0.4f, false, 0.3f) == A::startSprint);
    CHECK(tap.Process(keyboard, false, false, 0.5f, true, 0.3f) == A::pass);
    CHECK(tap.Process(keyboard, false, true, 0.6f, true, 0.3f) == A::pass);
    CHECK(tap.Process(keyboard, true, false, 0, true, 0.3f) == A::pass);
    tap.Reset();
    CHECK(tap.Process(keyboard, false, true, 0.1f, false, 0.3f) == A::pass);
    CHECK(tap.Process(keyboard, true, false, 0, false, 0.3f) == A::consume);
    tap.Reset();
    CHECK(tap.Process(keyboard, false, true, 0.1f, false, 0.3f) == A::pass);
    CHECK(tap.Process(keyboard, false, false, 0.4f, false, 0.3f) == A::pass);
    CHECK(tap.Process(keyboard, true, false, 0, false, 0) == A::startSprint);
    CHECK(tap.Process(keyboard, false, false, 0.1f, false, 0) == A::pass);
    CHECK(tap.Process(keyboard, false, true, 0.2f, false, 0) == A::pass);
    CHECK(tap.Process(keyboard, true, false, 0, false, 0.3f) == A::consume);
    CHECK(tap.Process(keyboard, false, true, 0.3f, false, 0.3f) == A::pass);

    CHECK(tap.Process(keyboard, true, false, 0, false, 0.3f) == A::consume);
    CHECK(tap.Process(gamepad, false, true, 0.1f, false, 0.3f) == A::consume);
    CHECK(tap.Process(gamepad, true, false, 0, false, 0.3f) == A::consume);
    CHECK(tap.Process(keyboard, false, true, 0.1f, false, 0.3f) == A::consume);
    CHECK(tap.Process(gamepad, false, true, 0.1f, false, 0.3f) == A::dodge);
    CHECK(tap.Process(keyboard, false, true, 0.1f, false, 0.3f) == A::pass);
    for (float bad : {NAN, INFINITY, -0.1f}) {
        CHECK(tap.Process(keyboard, true, false, 0, false, 0.3f) == A::consume);
        CHECK(tap.Process(keyboard, false, false, bad, false, 0.3f) == A::pass);
        CHECK(tap.Process(keyboard, false, true, 0.1f, false, 0.3f) == A::pass);
        CHECK(tap.Process(keyboard, true, false, 0, false, bad) == A::pass);
    }
    for (float threshold : {0.05f, 0.3f, 1.0f}) {
        unsigned starts = 0, dodges = 0;
        for (unsigned frame = 0; frame <= 100; ++frame) {
            const auto action = tap.Process(keyboard, frame == 0, frame == 100, frame * threshold / 50, false, threshold);
            starts += action == A::startSprint;
            dodges += action == A::dodge;
        }
        CHECK(starts == 1 && dodges == 0);
        starts = dodges = 0;
        for (unsigned frame = 0; frame <= 10; ++frame) {
            const auto action = tap.Process(gamepad, frame == 0, frame == 10, frame * threshold / 20, false, threshold);
            starts += action == A::startSprint;
            dodges += action == A::dodge;
        }
        CHECK(starts == 0 && dodges == 1);
    }

    SneakTap sneak;
    using S = SneakTap::Action;
    CHECK(sneak.Process(keyboard, false, true, 0.1f, 0.3f) == S::pass);
    CHECK(sneak.Process(keyboard, false, false, 0.4f, 0.3f) == S::pass);
    CHECK(sneak.Process(keyboard, true, false, 0, 0.3f) == S::consume);
    CHECK(sneak.Process(keyboard, false, false, 0.1f, 0.3f) == S::consume);
    CHECK(sneak.Process(keyboard, false, true, 0.2f, 0.3f) == S::dodge);
    CHECK(sneak.Process(keyboard, false, true, 0.2f, 0.3f) == S::pass);

    CHECK(sneak.Process(keyboard, true, false, 0, 0.3f) == S::consume);
    CHECK(sneak.Process(keyboard, false, false, 0.3f, 0.3f) == S::press);
    CHECK(sneak.Process(keyboard, false, false, 0.4f, 0.3f) == S::pass);
    CHECK(sneak.Process(keyboard, false, true, 0.5f, 0.3f) == S::pass);
    // Low frame rates can deliver a long release without an intermediate held event.
    CHECK(sneak.Process(keyboard, true, false, 0, 0.3f) == S::consume);
    CHECK(sneak.Process(keyboard, false, true, 0.3f, 0.3f) == S::press);
    CHECK(sneak.Process(keyboard, false, true, 0.3f, 0.3f) == S::pass);

    CHECK(sneak.Process(keyboard, true, false, 0, 0.3f) == S::consume);
    sneak.Reset();
    CHECK(sneak.Process(keyboard, false, true, 0.1f, 0.3f) == S::pass);
    CHECK(sneak.Process(keyboard, false, false, 0.4f, 0.3f) == S::pass);
    CHECK(sneak.Process(keyboard, true, false, 0, 0.3f) == S::consume);
    CHECK(sneak.Process(keyboard, false, false, 0.4f, 0.3f) == S::press);
    sneak.Reset();
    CHECK(sneak.Process(keyboard, false, true, 0.5f, 0.3f) == S::pass);

    CHECK(sneak.Process(keyboard, true, false, 0, 0.3f) == S::consume);
    CHECK(sneak.Process(gamepad, false, true, 0.1f, 0.3f) == S::consume);
    CHECK(sneak.Process(gamepad, true, false, 0, 0.3f) == S::consume);
    CHECK(sneak.Process(keyboard, false, true, 0.1f, 0.3f) == S::consume);
    CHECK(sneak.Process(gamepad, false, true, 0.1f, 0.3f) == S::dodge);
    CHECK(sneak.Process(gamepad, true, false, 0, 0.3f) == S::consume);
    CHECK(sneak.Process(gamepad, false, true, 0.1f, 0.3f) == S::dodge);

    CHECK(sneak.Process(keyboard, true, false, 0, 0) == S::press);
    CHECK(sneak.Process(keyboard, false, false, 0.1f, 0) == S::pass);
    CHECK(sneak.Process(keyboard, false, true, 0.2f, 0) == S::pass);
    for (float bad : {NAN, INFINITY, -0.1f}) {
        CHECK(sneak.Process(keyboard, true, false, 0, 0.3f) == S::consume);
        CHECK(sneak.Process(keyboard, false, false, bad, 0.3f) == S::pass);
        CHECK(sneak.Process(keyboard, false, true, 0.1f, 0.3f) == S::pass);
        CHECK(sneak.Process(keyboard, true, false, 0, bad) == S::pass);
    }
    for (float threshold : {0.05f, 0.3f, 1.0f}) {
        unsigned presses = 0, dodges = 0;
        for (unsigned frame = 0; frame <= 100; ++frame) {
            const auto action = sneak.Process(keyboard, frame == 0, frame == 100, frame * threshold / 50, threshold);
            presses += action == S::press;
            dodges += action == S::dodge;
        }
        CHECK(presses == 1 && dodges == 0);
        presses = dodges = 0;
        for (unsigned frame = 0; frame <= 10; ++frame) {
            const auto action = sneak.Process(gamepad, frame == 0, frame == 10, frame * threshold / 20, threshold);
            presses += action == S::press;
            dodges += action == S::dodge;
        }
        CHECK(presses == 0 && dodges == 1);
    }

    // Connection changes cancel both pending gestures without inventing a key-up.
    CHECK(tap.Process(gamepad, true, false, 0, false, 0.3f) == A::consume);
    CHECK(sneak.Process(gamepad, true, false, 0, 0.3f) == S::consume);
    tap.Reset();
    sneak.Reset();
    CHECK(tap.Process(gamepad, false, true, 0.1f, false, 0.3f) == A::pass);
    CHECK(sneak.Process(gamepad, false, true, 0.1f, 0.3f) == S::pass);
    CHECK(tap.Process(keyboard, true, false, 0, false, 0.3f) == A::consume);
    CHECK(sneak.Process(keyboard, true, false, 0, 0.3f) == S::consume);
    CHECK(tap.Process(keyboard, false, true, 0.1f, false, 0.3f) == A::dodge);
    CHECK(sneak.Process(keyboard, false, true, 0.1f, 0.3f) == S::dodge);

    // A fresh keydown also recovers a lost release without a connection event.
    CHECK(tap.Process(gamepad, true, false, 0, false, 0.3f) == A::consume);
    CHECK(sneak.Process(gamepad, true, false, 0, 0.3f) == S::consume);
    CHECK(tap.Process(gamepad, false, false, 0.4f, false, 0.3f) == A::startSprint);
    CHECK(sneak.Process(gamepad, false, false, 0.4f, 0.3f) == S::press);
    CHECK(tap.Process(keyboard, true, false, 0, false, 0.3f) == A::consume);
    CHECK(sneak.Process(keyboard, true, false, 0, 0.3f) == S::consume);
    CHECK(tap.Process(gamepad, false, true, 0.5f, false, 0.3f) == A::consume);
    CHECK(sneak.Process(gamepad, false, true, 0.5f, 0.3f) == S::consume);
    CHECK(tap.Process(keyboard, false, true, 0.1f, false, 0.3f) == A::dodge);
    CHECK(sneak.Process(keyboard, false, true, 0.1f, 0.3f) == S::dodge);

    struct ButtonState { float value; float heldDownSecs; } button{0.75f, 0.5f};
    static_assert(!std::is_copy_constructible_v<ScopedButtonState<ButtonState>>);
    static_assert(!std::is_copy_assignable_v<ScopedButtonState<ButtonState>>);
    auto delayedSprint = [&] {
        const ScopedButtonState restore{button};
        button.heldDownSecs = 0;
        return button.value == 0.75f && button.heldDownSecs == 0;
    };
    CHECK(delayedSprint());
    CHECK(button.value == 0.75f && button.heldDownSecs == 0.5f);
    {
        const ScopedButtonState restore{button};
        button.value = 1;
        button.heldDownSecs = 0;
        {
            const ScopedButtonState nested{button};
            button.value = 0;
            button.heldDownSecs = 2;
        }
        CHECK(button.value == 1 && button.heldDownSecs == 0);
    }
    CHECK(button.value == 0.75f && button.heldDownSecs == 0.5f);
    try {
        const ScopedButtonState restore{button};
        button.value = 0;
        button.heldDownSecs = 0;
        throw std::runtime_error("native test callback failed");
    } catch (const std::runtime_error&) {}
    CHECK(button.value == 0.75f && button.heldDownSecs == 0.5f);
    using TKDodge::config::Values;
    Values settings;
    CHECK(TKDodge::config::Valid(settings));
    for (float invalid : {NAN, INFINITY, -1.0f}) {
        auto invalidSettings = settings;
        invalidSettings.dodgeStamina = invalid;
        CHECK(!TKDodge::config::Valid(invalidSettings));
        invalidSettings = settings;
        invalidSettings.iFrameDuration = invalid;
        CHECK(!TKDodge::config::Valid(invalidSettings));
        invalidSettings = settings;
        invalidSettings.SprintingPressDuration = invalid;
        CHECK(!TKDodge::config::Valid(invalidSettings));
        invalidSettings = settings;
        invalidSettings.SneakingPressDuration = invalid;
        CHECK(!TKDodge::config::Valid(invalidSettings));
    }
    const auto testRoot = std::filesystem::current_path() / "build" / "settings-tests" / std::to_string(GetCurrentProcessId());
    std::filesystem::create_directories(testRoot);
    const auto ini = testRoot / "settings.ini";
    CHECK(!TKDodge::config::Load(ini, settings));
    CHECK(TKDodge::config::Load(ini, settings, true));
    CHECK(settings == Values{});
    {
        std::ofstream file(ini);
        file << "; retained comment\n[Main]\nDodgeHotkey=272\nStepDodge=true\nEnableSprintKeyDodge=true\n"
                "EnableSneakKeyDodge=true\nDodgeStamina=25.5\nEnableSneakDodge=true\nEnableDodgeAttackCancel=false\n"
                "iFrameDuration=0.125\nSprintingPressDuration=0.2\nSneakingPressDuration=0.4\nEnableDebugLog=true\n"
                "defaultDodgeEvent=CustomEvent\nUnknownOption=keep\n[AnotherSection]\nForeign=preserved\n";
    }
    CHECK(TKDodge::config::Load(ini, settings));
    CHECK(settings.dodgeKey == 272 && settings.stepDodge && settings.EnableSprintKeyDodge && settings.EnableSneakKeyDodge);
    CHECK(settings.dodgeStamina == 25.5f && settings.enableSneakDodge && !settings.enableDodgeAttackCancel);
    CHECK(settings.iFrameDuration == 0.125f && settings.SprintingPressDuration == 0.2f && settings.SneakingPressDuration == 0.4f);
    CHECK(settings.debugLog && settings.defaultDodgeEvent == "CustomEvent");
    settings.dodgeKey = UINT32_MAX;
    settings.SneakingPressDuration = 0.000000123456789f;
    CHECK(TKDodge::config::Save(ini, settings));
    Values roundtrip;
    CHECK(TKDodge::config::Load(ini, roundtrip));
    CHECK(roundtrip == settings);
    const auto read = [&] { std::ifstream stream(ini, std::ios::binary); return std::string(std::istreambuf_iterator<char>(stream), {}); };
    auto previous = read();
    CHECK(previous.find("retained comment") != previous.npos);
    CHECK(previous.find("UnknownOption") != previous.npos && previous.find("Foreign") != previous.npos);
    for (std::uint32_t key : {0u, 42u, 255u, 256u, 265u, 266u, 281u, UINT32_MAX}) {
        settings.dodgeKey = key;
        CHECK(TKDodge::config::Save(ini, settings));
        CHECK(TKDodge::config::Load(ini, roundtrip));
        CHECK(roundtrip == settings);
    }
    previous = read();
    auto invalidSettings = settings;
    invalidSettings.dodgeKey = 282;
    CHECK(!TKDodge::config::Save(ini, invalidSettings));
    CHECK(read() == previous);
    CHECK(!TKDodge::config::Save(testRoot / "absent" / "settings.ini", settings));
    const auto locked = CreateFileW(ini.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    CHECK(locked != INVALID_HANDLE_VALUE);
    settings.dodgeStamina = 99;
    const bool lockSaved = TKDodge::config::Save(ini, settings);
    CloseHandle(locked);
    CHECK(!lockSaved);
    CHECK(read() == previous);
    unsigned temporaryFiles{};
    for (const auto& entry : std::filesystem::directory_iterator(testRoot)) temporaryFiles += entry.path().extension() == ".tmp";
    CHECK(temporaryFiles == 0);
    {
        std::ofstream file(ini);
        file << "[Main]\nDodgeHotkey=-2\nDodgeStamina=nan\niFrameDuration=-3\n";
    }
    roundtrip = Values{};
    CHECK(TKDodge::config::Load(ini, roundtrip));
    CHECK(roundtrip.dodgeKey == 42 && roundtrip.dodgeStamina == 10 && roundtrip.iFrameDuration == 0);
    std::cout << "PASS: " << checks << " TK Dodge logic regression checks\n";
}
