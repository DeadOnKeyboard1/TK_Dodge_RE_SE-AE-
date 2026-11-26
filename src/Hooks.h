#pragma once

namespace Hooks
{
    bool install();
    bool ConfigureKeys(bool sprint, bool sneak);
    void Reset();

    class SprintHandlerHook
    {
    public:
        static void Hook();
    private:
        static void ProcessButton(RE::SprintHandler*, RE::ButtonEvent*, RE::PlayerControlsData*);
        static inline REL::Relocation<decltype(ProcessButton)> original;
    };

    class SneakHandlerHook
    {
    public:
        static void Hook();
    private:
        static void ProcessButton(RE::SneakHandler*, RE::ButtonEvent*, RE::PlayerControlsData*);
        static void Press(RE::SneakHandler*, RE::ButtonEvent*, RE::PlayerControlsData*);
        static inline REL::Relocation<decltype(ProcessButton)> original;
    };
}
