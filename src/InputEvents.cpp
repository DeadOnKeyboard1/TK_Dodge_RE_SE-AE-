#include "InputEvents.h"
#include "TKRE.h"
#include "AnimationEvents.h"
#include "Hooks.h"
#include "Menu.h"
#include "DodgeLogic.h"

namespace
{
	void TraceHotkey(RE::InputEvent* event, const char* state)
	{
		static TKDodge::logic::TraceBudget traces;
		const auto settings = Settings::Get();
		for (; event; event = event->next) {
			const auto button = event->AsButtonEvent();
			if (button && button->IsDown() && button->HasIDCode() && settings->dodgeKey != UINT32_MAX &&
			    InputEventHandler::GetButtonID(button) == settings->dodgeKey && traces.Take()) {
				INFO("Configured dodge key received: {}; sprintKeyMode={}", state, settings->EnableSprintKeyDodge);
			}
		}
	}
}

std::uint32_t InputEventHandler::GetGamepadIndex(RE::BSWin32GamepadDevice::Key a_key)
{
	using Key = RE::BSWin32GamepadDevice::Key;

	std::uint32_t index;
	switch (a_key) {
	case Key::kUp:
		index = 0;
		break;
	case Key::kDown:
		index = 1;
		break;
	case Key::kLeft:
		index = 2;
		break;
	case Key::kRight:
		index = 3;
		break;
	case Key::kStart:
		index = 4;
		break;
	case Key::kBack:
		index = 5;
		break;
	case Key::kLeftThumb:
		index = 6;
		break;
	case Key::kRightThumb:
		index = 7;
		break;
	case Key::kLeftShoulder:
		index = 8;
		break;
	case Key::kRightShoulder:
		index = 9;
		break;
	case Key::kA:
		index = 10;
		break;
	case Key::kB:
		index = 11;
		break;
	case Key::kX:
		index = 12;
		break;
	case Key::kY:
		index = 13;
		break;
	case Key::kLeftTrigger:
		index = 14;
		break;
	case Key::kRightTrigger:
		index = 15;
		break;
	default:
		index = kInvalid;
		break;
	}

	return index != kInvalid ? index + kGamepadOffset : kInvalid;
}

std::uint32_t InputEventHandler::GetButtonID(RE::ButtonEvent* a_event)
{
	if (!a_event) return kInvalid;
	auto id = a_event->GetIDCode();
	using DeviceType = RE::INPUT_DEVICE;
	switch (a_event->device.get()) {
	case DeviceType::kMouse:
		if (id > 9) return kInvalid;
		id += kMouseOffset;
		break;
	case DeviceType::kKeyboard:
		if (id > 255) return kInvalid;
		id += kKeyboardOffset;
		break;
	case DeviceType::kGamepad:
		id = GetGamepadIndex(static_cast<RE::BSWin32GamepadDevice::Key>(id));
		break;
	default:
		return kInvalid;
	}
	return id;
}



EventResult InputEventHandler::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*)
{
	using EventType = RE::INPUT_EVENT_TYPE;
	
	if (!a_event) 
		return EventResult::kContinue;

	const auto ui = RE::UI::GetSingleton();
	if (TKDodge::Menu::BlocksInput() || !ui || ui->GameIsPaused()) {
		TraceHotkey(*a_event, "blocked by menu/capture/pause");
		Hooks::Reset();
		return EventResult::kContinue;
	}
	
	const auto ctrlMap = RE::ControlMap::GetSingleton();
	if (!ctrlMap || ctrlMap->GetRuntimeData().contextPriorityStack.empty() || ctrlMap->GetRuntimeData().contextPriorityStack.back() != RE::UserEvents::INPUT_CONTEXT_ID::kGameplay) {
		TraceHotkey(*a_event, "blocked by non-gameplay/empty input context");
		Hooks::Reset();
		return EventResult::kContinue;
	}
	TraceHotkey(*a_event, "gameplay input");

	const auto settings = Settings::Get();
	for (auto event = *a_event; event; event = event->next) {
		if (event->eventType == EventType::kDeviceConnect) {
			Hooks::Reset();
			continue;
		}
		if (event->eventType != EventType::kButton)
			continue;

		const auto button = static_cast<RE::ButtonEvent*>(event);
		if (!button || !button->IsDown()) {
			continue;
		}

		if (!button->HasIDCode()) {
			continue;
		}

		const auto id = GetButtonID(button);
		if (!settings->EnableSprintKeyDodge && id != kInvalid && id == settings->dodgeKey) {
			const auto events = RE::UserEvents::GetSingleton();
			if (settings->EnableSneakKeyDodge && events && button->GetUserEvent() == events->sneak) continue;
			TKRE::dodge();
		}

	}

	return EventResult::kContinue;
}





