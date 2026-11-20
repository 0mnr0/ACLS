#include "common.hpp"
#include "core/commands/Commands.hpp"
#include "core/commands/HotkeySystem.hpp"
#include "core/dma/DmaMemory.hpp"
#include "core/dma/IMemory.hpp"
#include "core/filemgr/FileMgr.hpp"
#include "core/frontend/Notifications.hpp"
#include "core/logger/LogHelper.hpp"
#include "core/renderer/Renderer.hpp"
#include "core/settings/Settings.hpp"
#include "game/backend/DmaSession.hpp"
#include "game/frontend/GUI.hpp"

int main()
{
	using namespace YimMenu;

	std::filesystem::path appdata = ".";
	if (const char* appdataEnv = std::getenv("appdata"))
		appdata = appdataEnv;

	FileMgr::Init(appdata / "SafeYimV2_DMA");
	LogHelper::Init("SafeYimV2 DMA", FileMgr::GetProjectFile("./cout.log"));

	LOG(INFO) << "Starting SafeYimV2 DMA...";

	g_Memory = std::make_unique<DmaMemory>();
	g_HotkeySystem.RegisterCommands();
	Settings::Initialize(FileMgr::GetProjectFile("./settings.json"));

	if (!Renderer::Init())
	{
		LOG(FATAL) << "Failed to initialize standalone DX11 renderer.";
		LogHelper::Destroy();
		return EXIT_FAILURE;
	}

	GUI::Init();
	Commands::EnableBoolCommands();
	DmaSession::Init();

	Notifications::Show("SafeYimV2 DMA", "Standalone DMA client started", NotificationType::Success);

	while (g_Running && Renderer::Tick())
	{
		Settings::Tick();
	}

	LOG(INFO) << "Shutting down SafeYimV2 DMA...";
	g_Running = false;

	DmaSession::Shutdown();
	Commands::Shutdown();
	Renderer::Destroy();
	LogHelper::Destroy();

	return EXIT_SUCCESS;
}
