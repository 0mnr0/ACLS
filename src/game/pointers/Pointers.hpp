#pragma once
#include <cstdint>
#include <string>

namespace YimMenu
{
	struct PointerData
	{
		std::uintptr_t GameVersionAddr{};
		std::uintptr_t OnlineVersionAddr{};
		std::uintptr_t PedFactory{};
		std::uintptr_t NetworkPlayerMgr{};
		std::uintptr_t IsSessionStarted{};
		std::uintptr_t ScriptGlobals{};
		std::uintptr_t ScriptPrograms{};

		std::string GameVersion{};
		std::string OnlineVersion{};
	};

	struct Pointers : PointerData
	{
		bool Initialized{false};

		bool Init();
		void Reset();
	};

	inline YimMenu::Pointers Pointers;
}
