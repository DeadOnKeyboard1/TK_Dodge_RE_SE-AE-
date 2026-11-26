#pragma once

class animEventHandler
{
public:
    static void RegisterForPlayer();

private:
    using Sink = RE::BSTEventSink<RE::BSAnimationGraphEvent>;
    using Source = RE::BSTEventSource<RE::BSAnimationGraphEvent>;
    using Callback = RE::BSEventNotifyControl (*)(Sink*, const RE::BSAnimationGraphEvent*, Source*);
    static RE::BSEventNotifyControl ProcessEvent(Sink*, const RE::BSAnimationGraphEvent*, Source*);
    static inline Callback original{};
};
