#include "Hooks.h"
#include "AnimationEvents.h"
#include "DodgeLogic.h"
#include "TKRE.h"
#include "Menu.h"

namespace Hooks
{
    namespace
    {
        TKDodge::logic::SprintTap sprintTap;
        TKDodge::logic::SneakTap sneakTap;
        bool sprintHooked{};
        bool sneakHooked{};

        std::uint64_t InputKey(const RE::ButtonEvent* event)
        {
            return (static_cast<std::uint64_t>(event->device.get()) << 32) | event->GetIDCode();
        }

        bool GameplayInput()
        {
            const auto ui = RE::UI::GetSingleton();
            const auto controls = RE::ControlMap::GetSingleton();
            if (TKDodge::Menu::BlocksInput() || !ui || ui->GameIsPaused() || !controls || !controls->IsMovementControlsEnabled()) return false;
            const auto& contexts = controls->GetRuntimeData().contextPriorityStack;
            return !contexts.empty() && contexts.back() == RE::UserEvents::INPUT_CONTEXT_ID::kGameplay;
        }

        bool Executable(std::uintptr_t address)
        {
            MEMORY_BASIC_INFORMATION page{};
            if (!VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) || page.State != MEM_COMMIT || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
            return (page.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
        }

        std::size_t ButtonSlot()
        {
            return !REL::Module::IsVR() && REL::Module::IsAtLeast(SKSE::RUNTIME_SSE_1_7_99) ? 6 : 4;
        }

        bool ValidSlot(std::uintptr_t table, std::size_t slot)
        {
            MEMORY_BASIC_INFORMATION page{};
            const auto location = table + slot * sizeof(std::uintptr_t);
            if (!VirtualQuery(reinterpret_cast<void*>(location), &page, sizeof(page)) || page.State != MEM_COMMIT || (page.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
            if (location + sizeof(std::uintptr_t) > reinterpret_cast<std::uintptr_t>(page.BaseAddress) + page.RegionSize) return false;
            return Executable(*reinterpret_cast<std::uintptr_t*>(location));
        }
    }

    bool install()
    {
        static bool installed{};
        if (installed) return true;
        REL::Relocation<std::uintptr_t> animationTable{RE::VTABLE_PlayerCharacter[2]};
        REL::Relocation<std::uintptr_t> sprintTable{RE::VTABLE_SprintHandler[0]};
        if (!ValidSlot(animationTable.address(), 1) || !ValidSlot(sprintTable.address(), ButtonSlot())) {
            ERROR("Hook target validation failed; no TK Dodge hooks installed");
            return false;
        }
        const auto settings = Settings::Get();
        if (!ConfigureKeys(settings->EnableSprintKeyDodge, settings->EnableSneakKeyDodge)) return false;
        animEventHandler::RegisterForPlayer();
        installed = true;
        INFO("Hook installation complete; sprint button slot {}", ButtonSlot());
        return true;
    }

    void Reset() { sprintTap.Reset(); sneakTap.Reset(); }

    bool ConfigureKeys(bool sprint, bool sneak)
    {
        if (sprint && !sprintHooked) {
            REL::Relocation<std::uintptr_t> table{RE::VTABLE_SprintHandler[0]};
            if (!ValidSlot(table.address(), ButtonSlot())) return false;
        }
        if (sneak && !sneakHooked) {
            REL::Relocation<std::uintptr_t> table{RE::VTABLE_SneakHandler[0]};
            if (!ValidSlot(table.address(), ButtonSlot())) return false;
        }
        if (sprint && !sprintHooked) SprintHandlerHook::Hook();
        if (sneak && !sneakHooked) SneakHandlerHook::Hook();
        Reset();
        return true;
    }

    void SprintHandlerHook::Hook()
    {
        REL::Relocation<std::uintptr_t> table{RE::VTABLE_SprintHandler[0]};
        original = reinterpret_cast<std::uintptr_t*>(table.address())[ButtonSlot()];
        table.write_vfunc(ButtonSlot(), ProcessButton);
        sprintHooked = true;
    }

    void SprintHandlerHook::ProcessButton(RE::SprintHandler* self, RE::ButtonEvent* event, RE::PlayerControlsData* data)
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        const auto events = RE::UserEvents::GetSingleton();
        const auto settings = Settings::Get();
        if (!self || !event || !data || !player || !events || !settings->EnableSprintKeyDodge ||
            !self->inputEventHandlingEnabled || !GameplayInput()) {
            sprintTap.Reset();
            original(self, event, data);
            return;
        }
        if (event->GetUserEvent() != events->sprint) {
            original(self, event, data);
            return;
        }
        using Action = TKDodge::logic::SprintTap::Action;
        const auto action = sprintTap.Process(InputKey(event), event->IsDown(), event->Value() == 0.0f,
                                             event->HeldDuration(), player->AsActorState()->IsSprinting(), settings->SprintingPressDuration);
        if (action == Action::dodge) { TKRE::dodge(); return; }
        if (action == Action::consume) return;
        if (action == Action::startSprint) {
            const TKDodge::logic::ScopedButtonState restore{event->GetRuntimeData()};
            event->GetRuntimeData().heldDownSecs = 0.0f;
            original(self, event, data);
            return;
        }
        original(self, event, data);
    }

    void SneakHandlerHook::Hook()
    {
        REL::Relocation<std::uintptr_t> table{RE::VTABLE_SneakHandler[0]};
        original = reinterpret_cast<std::uintptr_t*>(table.address())[ButtonSlot()];
        table.write_vfunc(ButtonSlot(), ProcessButton);
        sneakHooked = true;
        INFO("Sneak-key hook installed");
    }

    void SneakHandlerHook::Press(RE::SneakHandler* self, RE::ButtonEvent* event, RE::PlayerControlsData* data)
    {
        // The native handler only toggles on a down edge. Restore shared event data
        // before another input sink can see our delayed press as a second keydown.
        auto& runtime = event->GetRuntimeData();
        const TKDodge::logic::ScopedButtonState restore{runtime};
        runtime.value = 1.0f;
        runtime.heldDownSecs = 0.0f;
        original(self, event, data);
    }

    void SneakHandlerHook::ProcessButton(RE::SneakHandler* self, RE::ButtonEvent* event, RE::PlayerControlsData* data)
    {
        const auto events = RE::UserEvents::GetSingleton();
        const auto settings = Settings::Get();
        if (!self || !event || !data || !events || !settings->EnableSneakKeyDodge ||
            !self->inputEventHandlingEnabled || !GameplayInput()) {
            sneakTap.Reset();
            original(self, event, data);
            return;
        }
        if (event->GetUserEvent() != events->sneak) {
            original(self, event, data);
            return;
        }
        using Action = TKDodge::logic::SneakTap::Action;
        const auto action = sneakTap.Process(InputKey(event), event->IsDown(), event->Value() == 0.0f,
                                            event->HeldDuration(), settings->SneakingPressDuration);
        if (action == Action::consume) return;
        if (action == Action::dodge && TKRE::dodge()) return;
        if (action == Action::press || action == Action::dodge) {
            Press(self, event, data);
            if (event->Value() == 0.0f) original(self, event, data);
            return;
        }
        original(self, event, data);
    }
}
