#include "Hooks.h"
#include "InputEvents.h"
#include "TKRE.h"
#include "AnimationEvents.h"
#include "Menu.h"

namespace TKDodge
{
#define SETTINGFILE_PATH "Data\\SKSE\\Plugins\\TK Dodge RE.ini"

	void EventCallback(SKSE::MessagingInterface::Message* msg)
	{
		if (!msg) return;
		if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
			Settings::readSettings();
			TKRE::Initialize();
			if (Hooks::install()) {
				InputEventHandler::Register();
				Menu::Register();
			}
		} else if (msg->type == SKSE::MessagingInterface::kPreLoadGame || msg->type == SKSE::MessagingInterface::kNewGame) {
			Hooks::Reset();
		}
	}
}
