#include "Menu.h"
#include "TKRE.h"
#include "Hooks.h"
#include "InputEvents.h"
#include "SettingsStore.h"
#include <SKSEMenuFramework.h>
#include <algorithm>
#include <optional>
#include <mutex>

namespace TKDodge::Menu
{
    namespace
    {
        namespace GUI = ImGuiMCP;
        using Values = config::Values;
        using Window = SKSEMenuFramework::Model::WindowInterface;
        std::atomic<Window*> mainWindow{};
        std::atomic<bool> capturing{}, resetRequested{};
        std::atomic<std::int64_t> captured{-1};
        std::atomic<std::uint64_t> swallowed{UINT64_MAX};
        std::atomic<bool> busy{};
        struct Result { Values value; std::string status; bool success; };
        std::mutex resultMutex;
        std::optional<Result> result;

        void Finish(const Values& value, std::string status, bool success)
        {
            {
                std::lock_guard lock(resultMutex);
                result = Result{value, std::move(status), success};
            }
        }

        void Request(Values value, bool reload)
        {
            if (busy.exchange(true)) return;
            const auto tasks = SKSE::GetTaskInterface();
            if (!tasks) { Finish(value, "Game task queue unavailable.", false); return; }
            tasks->AddTask([value = std::move(value), reload]() mutable {
                if (reload && !config::Load(config::iniPath, value)) { Finish(value, "INI read failed. Settings unchanged.", false); return; }
                if (!config::Valid(value)) { Finish(value, "Invalid setting. Settings unchanged.", false); return; }
                if (!Hooks::ConfigureKeys(value.EnableSprintKeyDodge, value.EnableSneakKeyDodge)) {
                    Finish(value, "Input hook validation failed. Settings unchanged.", false);
                    return;
                }
                if (!reload && !config::Save(config::iniPath, value)) { Finish(value, "INI write failed. Settings unchanged.", false); return; }
                const auto old = Settings::Get();
                auto runtime = value;
                runtime.stepDodge = old->stepDodge;
                Settings::Apply(runtime);
                Finish(value, value.stepDodge != runtime.stepDodge ? "Saved. Restart Skyrim to change roll/step mode." :
                              reload ? "INI reloaded." : "Applied and saved.", true);
            });
        }

        std::string KeyName(std::uint32_t key)
        {
            if (key == UINT32_MAX) return "Unbound";
            if (key > 281) return "Invalid key";
            if (key >= 266 && key <= 281) {
                constexpr const char* names[]{"D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right", "Start", "Back",
                    "Left Stick", "Right Stick", "Left Shoulder", "Right Shoulder", "A", "B", "X", "Y", "Left Trigger", "Right Trigger"};
                return std::string("Gamepad ") + names[key - 266];
            }
            if (key >= 256) {
                constexpr const char* names[]{"Left Mouse", "Right Mouse", "Middle Mouse", "Mouse 4", "Mouse 5",
                    "Mouse 6", "Mouse 7", "Mouse 8", "Wheel Up", "Wheel Down"};
                return names[key - 256];
            }
            wchar_t text[128]{};
            const auto scan = static_cast<LONG>(((key & 0x7f) << 16) | ((key & 0x80) ? (1 << 24) : 0));
            const auto length = GetKeyNameTextW(scan, text, 128);
            const auto bytes = length ? WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr) : 0;
            if (!bytes) return "Scan code " + std::to_string(key);
            std::string result(bytes, '\0');
            WideCharToMultiByte(CP_UTF8, 0, text, length, result.data(), bytes, nullptr, nullptr);
            return result;
        }

        bool __stdcall OnInput(RE::InputEvent* event)
        {
            if (!event) return false;
            if (event->eventType == RE::INPUT_EVENT_TYPE::kDeviceConnect) {
                capturing = false;
                captured = -2;
                swallowed = UINT64_MAX;
                resetRequested = true;
                return false;
            }
            const auto button = event->AsButtonEvent();
            if (!button) return false;
            const auto token = (static_cast<std::uint64_t>(button->device.get()) << 32) | button->GetIDCode();
            if (swallowed.load() == token) {
                if (button->Value() == 0) swallowed = UINT64_MAX;
                return true;
            }
            const auto window = mainWindow.load();
            if (!window || !window->IsOpen.load() || !capturing.load() || !button->IsDown()) return false;
            const auto code = InputEventHandler::GetButtonID(button);
            if (code == UINT32_MAX) return false;
            if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard && code == 1) captured = -2;
            else captured = code;
            const bool wheel = button->GetDevice() == RE::INPUT_DEVICE::kMouse && button->GetIDCode() >= 8;
            swallowed = wheel ? UINT64_MAX : token;
            capturing = false;
            resetRequested = true;
            return true;
        }

        void __stdcall OnEvent(SKSEMenuFramework::Model::EventType event)
        {
            if (event == SKSEMenuFramework::Model::kOpenMenu || event == SKSEMenuFramework::Model::kCloseMenu) {
                resetRequested = true;
                capturing = false;
                captured = -2;
                swallowed = UINT64_MAX;
            }
        }

        bool FloatSetting(const char* name, float& value, float range, const char* format)
        {
            return GUI::SliderFloat(name, &value, 0, (std::max)(range, value), format);
        }

        void __stdcall Render()
        {
            // Only this render callback owns the draft. Game tasks publish short
            // completion messages, never references into the ImGui draft.
            static Values draft, saved;
            static std::string status;
            {
                std::lock_guard lock(resultMutex);
                if (result) {
                    if (result->success) draft = saved = result->value;
                    status = std::move(result->status);
                    result.reset();
                    busy = false;
                }
            }
            const auto key = captured.exchange(-1);
            if (key >= 0) { draft.dodgeKey = static_cast<std::uint32_t>(key); status = "Unsaved changes."; }
            bool edited{};
            GUI::TextUnformatted("TK Dodge RE");
            GUI::BeginDisabled(busy.load() || capturing.load());
            if (GUI::BeginTabBar("TKRESettings")) {
                if (GUI::BeginTabItem("Dodge")) {
                    edited |= GUI::Checkbox("Step dodge (restart required)", &draft.stepDodge);
                    edited |= GUI::Checkbox("Allow dodge while sneaking", &draft.enableSneakDodge);
                    edited |= GUI::Checkbox("Allow attack cancel", &draft.enableDodgeAttackCancel);
                    edited |= FloatSetting("Stamina cost", draft.dodgeStamina, 200, "%.1f");
                    edited |= FloatSetting("Invulnerability duration", draft.iFrameDuration, 2, "%.3f s");
                    GUI::EndTabItem();
                }
                if (GUI::BeginTabItem("Controls")) {
                    GUI::BeginDisabled(draft.EnableSprintKeyDodge);
                    const auto binding = capturing.load() ? std::string("Waiting for input...") : KeyName(draft.dodgeKey);
                    if (GUI::Button((binding + "##BindDodge").c_str(), {220, 0})) { captured = -1; capturing = true; }
                    GUI::SetItemTooltip("Escape cancels binding.");
                    GUI::SameLine();
                    if (GUI::Button("Unbind")) { draft.dodgeKey = UINT32_MAX; edited = true; }
                    int code = draft.dodgeKey == UINT32_MAX ? -1 : static_cast<int>(draft.dodgeKey);
                    if (GUI::InputInt("Dodge hotkey", &code, 1, 10)) {
                        draft.dodgeKey = static_cast<std::uint32_t>(code);
                        edited = true;
                    }
                    GUI::EndDisabled();
                    edited |= GUI::Checkbox("Sprint-key dodge (replaces hotkey)", &draft.EnableSprintKeyDodge);
                    GUI::BeginDisabled(!draft.EnableSprintKeyDodge);
                    edited |= FloatSetting("Sprint hold threshold", draft.SprintingPressDuration, 2, "%.3f s");
                    GUI::EndDisabled();
                    edited |= GUI::Checkbox("Sneak-key dodge", &draft.EnableSneakKeyDodge);
                    GUI::BeginDisabled(!draft.EnableSneakKeyDodge);
                    edited |= FloatSetting("Sneak hold threshold", draft.SneakingPressDuration, 2, "%.3f s");
                    GUI::EndDisabled();
                    GUI::EndTabItem();
                }
                if (GUI::BeginTabItem("Advanced")) {
                    edited |= GUI::Checkbox("Debug logging", &draft.debugLog);
                    GUI::TextUnformatted("Fallback event (reserved)");
                    GUI::TextUnformatted(draft.defaultDodgeEvent.c_str());
                    GUI::SetItemTooltip("Reserved by the existing behavior. Stationary dodges are not enabled.");
                    GUI::EndTabItem();
                }
                GUI::EndTabBar();
            }
            if (edited) status = "Unsaved changes.";
            GUI::Separator();
            GUI::BeginDisabled(!config::Valid(draft) || draft == saved);
            if (GUI::Button("Apply & Save")) Request(draft, false);
            GUI::EndDisabled();
            GUI::SameLine();
            if (GUI::Button("Reload INI")) Request(*Settings::Get(), true);
            GUI::SameLine();
            if (GUI::Button("Defaults")) { draft = Values{}; draft.defaultDodgeEvent = saved.defaultDodgeEvent; status = "Unsaved changes."; }
            GUI::EndDisabled();
            GUI::TextUnformatted(!config::Valid(draft) ? "Invalid numeric value or key code." : busy.load() ? "Applying..." : status.c_str());
        }
    }

    bool BlocksInput()
    {
        if (resetRequested.exchange(false)) Hooks::Reset();
        const auto window = mainWindow.load();
        return capturing.load() || (window && window->IsOpen.load());
    }

    void Register()
    {
        static bool registered{};
        if (registered) return;
        const auto module = GetModuleHandleW(L"SKSEMenuFramework.dll");
        if (!module) { INFO("SKSE Menu Framework not loaded; INI-only configuration remains available"); return; }
        constexpr const char* required[]{"GetMenuFrameworkVersion", "GetMainWindow", "AddSectionItem", "RegisterInpoutEvent", "RegisterEventPriority",
            "igTextUnformatted", "igBeginDisabled", "igEndDisabled", "igBeginTabBar", "igEndTabBar", "igBeginTabItem", "igEndTabItem",
            "igCheckbox", "igSliderFloat", "igButton", "igSameLine", "igInputInt", "igSeparator", "igSetItemTooltipV"};
        for (const auto name : required) if (!GetProcAddress(module, name)) { INFO("Menu API export unavailable: {}", name); return; }
        const auto version = reinterpret_cast<float(*)()>(GetProcAddress(module, "GetMenuFrameworkVersion"))();
        const auto api = reinterpret_cast<std::uint32_t(*)()>(GetProcAddress(module, "GetMenuFrameworkAPIVersion"));
        if (!std::isfinite(version) || version < 3.4f || version >= 4 || (api && api() != 1)) {
            INFO("Menu API version not qualified; INI-only configuration retained");
            return;
        }
        const auto window = SKSEMenuFramework::GetMainWindow();
        if (!window) { INFO("Menu main window unavailable; INI-only configuration retained"); return; }
        mainWindow = window;
        Finish(*Settings::Get(), "Loaded from INI.", true);
        GetMenuFrameworkFunction<SKSEMenuFramework::Model::RegisterInputEventFuction>("RegisterInpoutEvent")(OnInput);
        GetMenuFrameworkFunction<SKSEMenuFramework::Model::RegisterEventFuction>("RegisterEventPriority")(OnEvent, 0);
        SKSEMenuFramework::SetSection("TK Dodge RE");
        SKSEMenuFramework::AddSectionItem("Settings", Render);
        registered = true;
        INFO("SKSE Menu Framework settings registered; API version {}", version);
    }
}
