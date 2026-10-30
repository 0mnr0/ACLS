#include "Pointers.hpp"

#include "core/backend/PatternCache.hpp"
#include "core/memory/ModuleMgr.hpp"
#include "core/memory/PatternScanner.hpp"
#include "core/util/Joaat.hpp"
#include "types/network/rlSessionInfo.hpp"
#include "types/rage/atArray.hpp"
#include "types/network/CNetworkPlayerMgr.hpp"
#include <chrono>

using namespace std::chrono_literals;

namespace YimMenu
{
	// on some cracked game builds
	static bool IsSocialClubNeverGoingToLoad()
	{
		return Pointers.ScriptThreads && Pointers.ScriptThreads->size() != 0;
	}

	bool Pointers::Init()
	{
		PatternCache::Init();

		const auto gta5 = ModuleMgr.Get("GTA5_Enhanced.exe"_J);
		if (!gta5)
		{
			LOG(FATAL) << "Could not find GTA5_Enhanced.exe, is this GTA 5 Enhanced?";
			return false;
		}

		auto scanner = PatternScanner(gta5);

		constexpr auto swapchainPtrn = Pattern<"72 C7 EB 02 31 C0 8B 0D">("IDXGISwapChain");
		scanner.Add(swapchainPtrn, [this](PointerCalculator ptr) {
			CommandQueue = ptr.Add(0x1A).Add(3).Rip().As<ID3D12CommandQueue**>();
			SwapChain = ptr.Add(0x21).Add(3).Rip().As<IDXGISwapChain1**>();
		});

		constexpr auto wndProcPtrn = Pattern<"3D 85 00 00 00 0F 87 2D 02 00 00">("WndProc");
		scanner.Add(wndProcPtrn, [this](PointerCalculator ptr) {
			WndProc = ptr.Sub(0x4F).As<PVOID>();
		});

		constexpr auto screenResPtrn = Pattern<"75 39 0F 57 C0 F3 0F 2A 05">("ScreenRes");
		scanner.Add(screenResPtrn, [this](PointerCalculator ptr) {
			ScreenResX = ptr.Add(0x5).Add(4).Rip().As<std::uint32_t*>();
			ScreenResY = ptr.Add(0x1E).Add(4).Rip().As<std::uint32_t*>();
		});

		constexpr auto versionPtrn = Pattern<"4C 8D 0D ? ? ? ? 48 8D 5C 24 ? 48 89 D9 48 89 FA">("Version");
		scanner.Add(versionPtrn, [this](PointerCalculator ptr) {
			GameVersion = ptr.Add(3).Rip().As<const char*>();
			OnlineVersion = ptr.Add(0x47).Add(3).Rip().As<const char*>();
		});

		constexpr auto scriptThreadsPtrn = Pattern<"48 8B 05 ? ? ? ? 48 89 34 F8 48 FF C7 48 39 FB 75 97">("ScriptThreads");
		scanner.Add(scriptThreadsPtrn, [this](PointerCalculator ptr) {
			ScriptThreads = ptr.Add(3).Rip().As<rage::atArray<rage::scrThread*>*>();
		});

		constexpr auto populateNativesPtrn = Pattern<"EB 2A 0F 1F 40 00 48 8B 54 17 10">("InitNativeTables");
		scanner.Add(populateNativesPtrn, [this](PointerCalculator ptr) {
			InitNativeTables = ptr.Sub(0x2A).As<PVOID>();
		});

		constexpr auto runScriptThreadsPtrn = Pattern<"BE 40 5D C6 00">("RunScriptThreads");
		scanner.Add(runScriptThreadsPtrn, [this](PointerCalculator ptr) {
			RunScriptThreads = ptr.Sub(0xA).As<PVOID>();
		});

		constexpr auto handlesAndPtrsPtrn = Pattern<"0F 1F 84 00 00 00 00 00 89 F8 0F 28 FE 41">("HandlesAndPtrs");
		scanner.Add(handlesAndPtrsPtrn, [this](PointerCalculator ptr) {
			HandleToPtr = ptr.Add(0x21).Add(1).Rip().As<Functions::HandleToPtr>();
			PtrToHandle = ptr.Sub(0xB).Add(1).Rip().As<Functions::PtrToHandle>();
		});

		constexpr auto pedFactoryPtrn = Pattern<"C7 40 30 03 00 00 00 48 8B 0D">("PedFactory");
		scanner.Add(pedFactoryPtrn, [this](PointerCalculator ptr) {
			PedFactory = ptr.Add(7).Add(3).Rip().As<CPedFactory**>();
		});

		constexpr auto isSessionStarted = Pattern<"0F B6 05 ? ? ? ? 0A 05 ? ? ? ? 75 2A">("IsSessionStarted");
		scanner.Add(isSessionStarted, [this](PointerCalculator addr) {
			IsSessionStarted = addr.Add(3).Rip().As<bool*>();
		});

		constexpr auto networkPlayerMgrPtrn = Pattern<"75 0E 48 8B 05 ? ? ? ? 48 8B 88 F0 00 00 00">("NetworkPlayerMgr");
		scanner.Add(networkPlayerMgrPtrn, [this](PointerCalculator ptr) {
			NetworkPlayerMgr = ptr.Add(2).Add(3).Rip().As<CNetworkPlayerMgr**>();
		});

		constexpr auto queuePacketPtrn = Pattern<"45 89 F1 E8 ? ? ? ? 84 C0 74 25">("QueuePacket");
		scanner.Add(queuePacketPtrn, [this](PointerCalculator ptr) {
			QueuePacket = ptr.Add(3).Add(1).Rip().As<Functions::QueuePacket>();
		});

		constexpr auto scriptGlobalsPtrn = Pattern<"48 8B 8E B8 00 00 00 48 8D 15 ? ? ? ? 49 89 D8">("ScriptGlobals");
		scanner.Add(scriptGlobalsPtrn, [this](PointerCalculator ptr) {
			ScriptGlobals = ptr.Add(7).Add(3).Rip().As<std::int64_t**>();
		});

		constexpr auto scriptProgramsPtrn = Pattern<"48 C7 84 C8 D8 00 00 00 00 00 00 00">("ScriptPrograms");
		scanner.Add(scriptProgramsPtrn, [this](PointerCalculator ptr) {
			ScriptPrograms = ptr.Add(0x13).Add(3).Rip().Add(0xD8).As<rage::scrProgram**>();
		});

		constexpr auto scriptVMPtrn = Pattern<"49 63 41 1C">("ScriptVM");
		scanner.Add(scriptVMPtrn, [this](PointerCalculator ptr) {
			ScriptVM = ptr.Sub(0x24).As<Functions::ScriptVM>();
		});

		constexpr auto prepareMetricForSendingPtrn = Pattern<"41 56 56 57 55 53 48 83 EC ? 4C 89 CB 4C 89 C6">("PrepareMetricForSending");
		scanner.Add(prepareMetricForSendingPtrn, [this](PointerCalculator ptr) {
			PrepareMetricForSending = ptr.As<PVOID>();
		});

		constexpr auto beDataPtrn = Pattern<"48 C7 05 ? ? ? ? 00 00 00 00 E8 ? ? ? ? 48 89 C1 E8 ? ? ? ? E8 ? ? ? ? BD 0A 00 00 00">("BEData");
		scanner.Add(beDataPtrn, [this](PointerCalculator ptr) {
			BERestartStatus = ptr.Add(3).Rip().Add(8).Add(4).As<int*>();
			NeedsBERestart = ptr.Add(3).Rip().Add(8).Add(4).Add(8).As<bool*>();
			IsBEBanned = ptr.Add(3).Rip().Add(8).Add(4).Add(8).Add(4).As<bool*>();
		});

		constexpr auto httpStartRequestPtrn = Pattern<"56 57 48 83 EC 28 48 89 CE 8B 81 ? ? ? ? FF C8 83 F8 04 0F 87">("HttpStartRequest");
		scanner.Add(httpStartRequestPtrn, [this](PointerCalculator ptr) {
			HttpStartRequest = ptr.As<PVOID>();
		});

		constexpr auto battlEyeServerProcessPlayerJoinPtrn = Pattern<"48 89 10 48 89 48 10 89 F9">("BattlEyeServerProcessPlayerJoin");
		scanner.Add(battlEyeServerProcessPlayerJoinPtrn, [this](PointerCalculator ptr) {
			BattlEyeServerProcessPlayerJoin = ptr.Sub(4).Rip().As<PVOID*>()[1];
		});

		constexpr auto receiveNetMessagePtrn = Pattern<"48 81 C1 00 03 00 00 4C 89 E2">("ReceiveNetMessage");
		scanner.Add(receiveNetMessagePtrn, [this](PointerCalculator ptr) {
			ReceiveNetMessage = ptr.Add(0xD).Add(1).Rip().As<PVOID>();
		});

		constexpr auto anticheatInitializedHashPtrn = Pattern<"89 9E C8 00 00 00 48 8B 0D ? ? ? ? 48 85 C9 74 46">("AnticheatInitializedHash&GetAnticheatInitializedHash");
		scanner.Add(anticheatInitializedHashPtrn, [this](PointerCalculator ptr) {
			AnticheatInitializedHash = ptr.Add(9).Rip().As<rage::Obf32**>();
			GetAnticheatInitializedHash = ptr.Add(0x13).Rip().As<PVOID>();
		});

		constexpr auto anticheatContextPtrn = Pattern<"48 8D BB 70 0A 00 00 4C 8D 35 ? ? ? ? 66 90">("AnticheatContext");
		scanner.Add(anticheatContextPtrn, [this](PointerCalculator ptr) {
			AnticheatContext = ptr.Sub(0x12).Add(3).Rip().As<CAnticheatContext**>();
		});

		constexpr auto getAnticheatInitializedHash2Ptrn = Pattern<"89 9E E8 00 00 00 89 C2 E8 ? ? ? ? 69">("GetAnticheatInitializedHash2");
		scanner.Add(getAnticheatInitializedHash2Ptrn, [this](PointerCalculator ptr) {
			GetAnticheatInitializedHash2 = ptr.Add(0x9).Rip().As<PVOID>();
		});

		static constexpr auto gameSkeletonUpdatePtrn = Pattern<"56 48 83 EC 20 48 8B 81 40 01 00 00 48 85 C0">("GameSkeletonUpdate");
		scanner.Add(gameSkeletonUpdatePtrn, [this](PointerCalculator addr) {
			GameSkeletonUpdate = addr.As<PVOID>();
		});

		if (!scanner.Scan())
		{
			LOG(FATAL) << "Some patterns could not be found, unloading.";
			return false;
		}

		PatternCache::Update();
		return true;
	}

	bool Pointers::LateInit()
	{
		auto sc = ModuleMgr.Get("socialclub.dll"_J);
		while (!sc)
		{
			LOG(WARNING) << "Waiting for socialclub.dll";
			std::this_thread::sleep_for(1s);

			if (IsSocialClubNeverGoingToLoad())
			{
				LOG(WARNING) << "Timed out checking for socialclub.dll";
				return false;
			}

			ModuleMgr.LoadModules();
			sc = ModuleMgr.Get("socialclub.dll"_J);
		}

		auto scanner = PatternScanner(sc);

		constexpr auto numHandlesPatchPtrn = Pattern<"83 FD 20 0F 87 54 02 00 00">("NumHandlesPatch");
		scanner.Add(numHandlesPatchPtrn, [this](PointerCalculator ptr) {
			BytePatches::Add(ptr.Add(2).As<std::uint8_t*>(), 100)->Apply(); // change handle limit to 100
		});

		constexpr auto readAttributePatchPtrn = Pattern<"75 70 EB 23">("ReadAttributesPatch");
		scanner.Add(readAttributePatchPtrn, [this](PointerCalculator ptr) {
			BytePatches::Add(ptr.As<void*>(), std::to_array<std::uint8_t>({0x90, 0x90}))->Apply();
		});

		constexpr auto readAttributePatch2Ptrn = Pattern<"32 C0 EB ? C7 83">("ReadAttributesPatch2");
		scanner.Add(readAttributePatch2Ptrn, [this](PointerCalculator ptr) {
			BytePatches::Add(ptr.As<void*>(), std::to_array<std::uint8_t>({0xB0, 0x01}))->Apply();
		});

		if (!scanner.Scan())
		{
			LOG(WARNING) << "Some socialclub patterns could not be found";
			return false;
		}

		PatternCache::Update();
		return true;
	}
}
