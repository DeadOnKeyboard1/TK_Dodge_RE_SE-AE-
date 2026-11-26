#include "LoadGame.h"

DLLEXPORT constinit auto SKSEPlugin_Version = []() noexcept {
	SKSE::PluginVersionData data{};

	data.PluginVersion(Plugin::Version);
	data.PluginName(Plugin::NAME);
	data.AuthorName(Plugin::AUTHOR);
	data.UsesAddressLibrary();
	data.UsesUpdatedStructs();

	return data;
}();

DLLEXPORT bool SKSEPlugin_Query(const SKSE::QueryInterface* skse, SKSE::PluginInfo* pluginInfo)
{
	if (!skse || !pluginInfo || skse->IsEditor()) return false;
	pluginInfo->name = SKSEPlugin_Version.pluginName;
	pluginInfo->infoVersion = SKSE::PluginInfo::kVersion;
	pluginInfo->version = SKSEPlugin_Version.pluginVersion;

	return true;
}

DLLEXPORT bool SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
#ifndef NDEBUG
	while (!IsDebuggerPresent()) {
		Sleep(100);
	}
#endif

	if (!a_skse || a_skse->IsEditor()) return false;
	SKSE::Init(a_skse);

	INFO("{} v{} loaded", Plugin::NAME, Plugin::Version.string());

	// do stuff
	auto g_message = SKSE::GetMessagingInterface();
	if (!g_message) {
		ERROR("Messaging Interface Not Found!");
		return false;
	}

	return g_message->RegisterListener(TKDodge::EventCallback);
}
