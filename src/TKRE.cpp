#include "TKRE.h"
#include "DodgeLogic.h"
#include "SettingsStore.h"

#define SETTINGFILE_PATH "Data\\SKSE\\Plugins\\TK Dodge RE.ini"

#define PI 3.14159265f
#define PI8 0.39269908f

inline RE::NiPoint2 Vec2Rotate(const RE::NiPoint2& vec, float angle)
{
	RE::NiPoint2 ret;
	ret.x = vec.x * cos(angle) - vec.y * sin(angle);
	ret.y = vec.x * sin(angle) + vec.y * cos(angle);
	return ret;
}

inline float Vec2Length(const RE::NiPoint2& vec)
{
	return std::sqrtf(vec.x * vec.x + vec.y * vec.y);
}

inline RE::NiPoint2 Vec2Normalize(RE::NiPoint2& vec)
{
	RE::NiPoint2 ret(0.f, 0.f);
	float vecLength = Vec2Length(vec);
	if (vecLength == 0) {
		return ret;
	}
	float invlen = 1.0f / vecLength;
	ret.x = vec.x * invlen;
	ret.y = vec.y * invlen;
	return ret;
}

inline float DotProduct(RE::NiPoint2& a, RE::NiPoint2& b)
{
	return a.x * b.x + a.y * b.y;
}

inline float CrossProduct(RE::NiPoint2& a, RE::NiPoint2& b)
{
	return a.x * b.y - a.y * b.x;
}

inline float GetAngle(RE::NiPoint2& a, RE::NiPoint2& b)
{
	return atan2(CrossProduct(a, b), DotProduct(a, b));
}

inline bool isJumping(RE::Actor* a_actor)
{
	bool result = false;
	return a_actor->GetGraphVariableBool("bInJumpState", result) && result;
}

void TKRE::Initialize()
{
	const auto data = RE::TESDataHandler::GetSingleton();
	tdmDirectional = data ? data->LookupForm<RE::TESGlobal>(0x807, "TrueDirectionalMovement.esp") : nullptr;
}

bool TKRE::GetDodgeEvent(std::string& a_event)
{
	const auto controls = RE::PlayerControls::GetSingleton();
	if (!controls) return false;
	const auto input = controls->data.prevMoveVec;
	const auto direction = TKDodge::logic::Direction(input.x, input.y, tdmDirectional && tdmDirectional->value);
	if (!direction) return false;
	a_event = *direction;
	return true;
}

// Return the first existing eligibility gate without changing dodge rules.
inline const char* DodgeBlockReason(RE::PlayerCharacter* a_pc, const TKDodge::config::Values& settings)
{
	auto playerControls = RE::PlayerControls::GetSingleton();
	bool bIsDodging = false;
	auto controlMap = RE::ControlMap::GetSingleton();
	const auto ui = RE::UI::GetSingleton();
	if (!a_pc || !playerControls || !controlMap || !ui) return "game objects unavailable";
	if (ui->GameIsPaused()) return "game paused";
	if (a_pc->IsDead()) return "player dead";
	const auto state = a_pc->AsActorState();
	auto attackState = state->GetAttackState();
	if (!a_pc->GetGraphVariableBool("bIsDodging", bIsDodging)) return "behavior variable bIsDodging missing";
	if (bIsDodging) return "behavior bIsDodging already true";
	if (attackState != RE::ATTACK_STATE_ENUM::kNone && !settings.enableDodgeAttackCancel) return "attack cancel disabled";
	if (state->IsSprinting() && settings.EnableSprintKeyDodge) return "already sprinting in sprint-key mode";
	if (!controlMap->IsMovementControlsEnabled()) return "movement controls disabled";
	if (!controlMap->IsFightingControlsEnabled()) return "fighting controls disabled";
	if (state->IsSneaking() && !settings.enableSneakDodge) return "crouched dodge disabled";
	if (!playerControls->attackBlockHandler || !playerControls->attackBlockHandler->inputEventHandlingEnabled) return "attack input handler disabled";
	if (!playerControls->movementHandler || !playerControls->movementHandler->inputEventHandlingEnabled) return "movement input handler disabled";
	if (state->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal) return "sitting or sleeping";
	if (state->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal) return "knock state active";
	if (state->GetFlyState() != RE::FLY_STATE::kNone) return "fly state active";
	if (state->IsSwimming()) return "swimming";
	if (isJumping(a_pc)) return "jumping";
	if (a_pc->IsInKillMove()) return "killmove active";
	if (!(a_pc->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina) >= settings.dodgeStamina)) return "insufficient stamina";
	return nullptr;
}

bool TKRE::dodge()
{
	DEBUG("dodging");
	static TKDodge::logic::TraceBudget traces;
	const bool trace = traces.Take();
	const auto settings = Settings::Get();
	auto pc = RE::PlayerCharacter::GetSingleton();
	if (const auto reason = DodgeBlockReason(pc, *settings)) {
		if (trace) INFO("Dodge rejected: {}", reason);
		DEBUG("cannot dodge");
		return false;
	}
	std::string dodge_event = settings->defaultDodgeEvent;
	if (!GetDodgeEvent(dodge_event)) {
		if (trace) INFO("Dodge rejected: no valid movement direction; stationary dodge is not enabled");
		return false;
	}

	if (settings->stepDodge) {
		pc->SetGraphVariableInt("iStep", 2);
	}
	const bool iframe = pc->SetGraphVariableFloat("TKDR_IframeDuration", settings->iFrameDuration);
	const bool accepted = pc->NotifyAnimationGraph(dodge_event);
	if (trace) INFO("Dodge event={} accepted={} iframeVariable={} step={}", dodge_event, accepted, iframe, settings->stepDodge);
	return accepted;
}

void TKRE::applyDodgeCost()
{
	static TKDodge::logic::TraceBudget traces;
	if (traces.Take()) INFO("Behavior event received: TKDR_DodgeStart");
	auto pc = RE::PlayerCharacter::GetSingleton();
	if (pc && !pc->IsGodMode()) {
		pc->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, -Settings::Get()->dodgeStamina);
	}
}

bool Settings::Apply(const TKDodge::config::Values& value)
{
	if (!TKDodge::config::Valid(value)) return false;
	current.store(std::make_shared<const TKDodge::config::Values>(value));
	spdlog::set_level(value.debugLog ? spdlog::level::debug : spdlog::level::info);
	return true;
}

void Settings::readSettings()
{
	INFO("Reading Settings...");
	auto value = *Get();
	if (TKDodge::config::Load(TKDodge::config::iniPath, value, true)) Apply(value);
	else INFO("INI read failed; retaining previous settings");
	const auto active = Get();
	INFO("Active settings: key={} sprintKey={} sneakKey={} allowCrouched={} step={} stamina={}",
	     active->dodgeKey, active->EnableSprintKeyDodge, active->EnableSneakKeyDodge,
	     active->enableSneakDodge, active->stepDodge, active->dodgeStamina);
	INFO("...done");
}
