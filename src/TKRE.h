#pragma once
#include "SettingsModel.h"
#include <atomic>
#include <memory>
#include <cstddef>

class TKRE
{
public:
	static void Initialize();
	static bool GetDodgeEvent(std::string& a_event);

	static bool dodge();

	static void applyDodgeCost();

private:
	static inline RE::TESGlobal* tdmDirectional{};
	TKRE() = default;

	~TKRE() = default;

	TKRE(const TKRE&) = delete;

	TKRE(TKRE&&) = delete;

	TKRE& operator=(const TKRE&) = delete;

	TKRE& operator=(TKRE&&) = delete;
};

class Settings
{
public:
	static std::shared_ptr<const TKDodge::config::Values> Get() { return current.load(); }
	static bool Apply(const TKDodge::config::Values& value);
	static void readSettings();
private:
	static inline std::atomic<std::shared_ptr<const TKDodge::config::Values>> current{std::make_shared<const TKDodge::config::Values>()};
};
