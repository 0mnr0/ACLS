#include "Pointers.hpp"

#include "core/backend/PatternCache.hpp"
#include "core/dma/IMemory.hpp"
#include "core/memory/ModuleMgr.hpp"
#include "core/memory/PatternScanner.hpp"
#include "core/util/Joaat.hpp"

namespace YimMenu
{
	static std::string ReadRemoteCString(std::uintptr_t address, std::size_t maxLen = 64)
	{
		if (!g_Memory || !IMemory::IsValidUserAddress(address))
			return {};

		std::vector<char> buf(maxLen, '\0');
		if (!g_Memory->ReadBuffer(address, buf.data(), maxLen - 1, false))
			return {};

		return std::string(buf.data());
	}

	void Pointers::Reset()
	{
		*static_cast<PointerData*>(this) = PointerData{};
		Initialized = false;
	}

	bool Pointers::Init()
	{
		Reset();
		PatternCache::Init();

		const auto gta5 = ModuleMgr.Get("GTA5_Enhanced.exe"_J);
		if (!gta5)
		{
			LOG(WARNING) << "Could not find GTA5_Enhanced.exe, is this GTA 5 Enhanced?";
			return false;
		}

		auto scanner = PatternScanner(gta5);

		constexpr auto versionPtrn = Pattern<"4C 8D 0D ? ? ? ? 48 8D 5C 24 ? 48 89 D9 48 89 FA">("Version");
		scanner.Add(versionPtrn, [this](PointerCalculator ptr) {
			GameVersionAddr = ptr.Add(3).Rip().As<std::uintptr_t>();
			OnlineVersionAddr = ptr.Add(0x47).Add(3).Rip().As<std::uintptr_t>();
		});

		constexpr auto pedFactoryPtrn = Pattern<"C7 40 30 03 00 00 00 48 8B 0D">("PedFactory");
		scanner.Add(pedFactoryPtrn, [this](PointerCalculator ptr) {
			PedFactory = ptr.Add(7).Add(3).Rip().As<std::uintptr_t>();
		});

		constexpr auto networkPlayerMgrPtrn = Pattern<"75 0E 48 8B 05 ? ? ? ? 48 8B 88 F0 00 00 00">("NetworkPlayerMgr");
		scanner.Add(networkPlayerMgrPtrn, [this](PointerCalculator ptr) {
			NetworkPlayerMgr = ptr.Add(2).Add(3).Rip().As<std::uintptr_t>();
		});

		constexpr auto isSessionStarted = Pattern<"0F B6 05 ? ? ? ? 0A 05 ? ? ? ? 75 2A">("IsSessionStarted");
		scanner.Add(isSessionStarted, [this](PointerCalculator addr) {
			IsSessionStarted = addr.Add(3).Rip().As<std::uintptr_t>();
		});

		constexpr auto scriptGlobalsPtrn = Pattern<"48 8B 8E B8 00 00 00 48 8D 15 ? ? ? ? 49 89 D8">("ScriptGlobals");
		scanner.Add(scriptGlobalsPtrn, [this](PointerCalculator ptr) {
			ScriptGlobals = ptr.Add(7).Add(3).Rip().As<std::uintptr_t>();
		});

		constexpr auto scriptProgramsPtrn = Pattern<"48 C7 84 C8 D8 00 00 00 00 00 00 00">("ScriptPrograms");
		scanner.Add(scriptProgramsPtrn, [this](PointerCalculator ptr) {
			ScriptPrograms = ptr.Add(0x13).Add(3).Rip().Add(0xD8).As<std::uintptr_t>();
		});

		const bool scanOk = scanner.Scan();
		gta5->ClearCache();

		if (!scanOk)
		{
			LOG(WARNING) << "Some DMA patterns could not be found.";
			Reset();
			return false;
		}

		GameVersion = ReadRemoteCString(GameVersionAddr);
		OnlineVersion = ReadRemoteCString(OnlineVersionAddr);
		if (!GameVersion.empty())
			LOGF(INFO, "Detected GTA5_Enhanced GameVersion: {}, OnlineVersion: {}", GameVersion, OnlineVersion);

		PatternCache::Update();
		Initialized = true;
		return true;
	}
}
