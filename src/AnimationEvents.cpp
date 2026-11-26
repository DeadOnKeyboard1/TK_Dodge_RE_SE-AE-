#include "AnimationEvents.h"
#include "TKRE.h"

void animEventHandler::RegisterForPlayer()
{
    if (original) return;
    REL::Relocation<std::uintptr_t> table{RE::VTABLE_PlayerCharacter[2]};
    original = reinterpret_cast<Callback>(reinterpret_cast<std::uintptr_t*>(table.address())[1]);
    table.write_vfunc(1, ProcessEvent);
}

RE::BSEventNotifyControl animEventHandler::ProcessEvent(Sink* self, const RE::BSAnimationGraphEvent* event, Source* source)
{
    if (event && event->tag == "TKDR_DodgeStart") TKRE::applyDodgeCost();
    return original(self, event, source);
}
