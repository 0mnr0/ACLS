#pragma once
#include "types/script/scrNativeHandler.hpp"
#include "types/rage/ObfVar.hpp"
#include "core/memory/BytePatches.hpp"
#include <d3d12.h>
#include <dxgi1_4.h>

namespace rage
{
	template<typename T>
	class atArray;
	class scrThread;
	class scrProgram;
	class netConnectionManager;
}
class CPedFactory;
class CAnticheatContext;
class CNetworkPlayerMgr;

namespace YimMenu
{
	namespace Functions
	{
		using HandleToPtr = void* (*)(int handle);
		using PtrToHandle = int (*)(void* pointer);
		using ScriptVM = int (*)(uint64_t* stack, int64_t** scr_globals, rage::scrProgram* program, void* ctx);
		using QueuePacket = void (*)(rage::netConnectionManager* mgr, int msg_id, void* data, int size, int flags, std::uint16_t* out_seq_id);
	}

	struct PointerData
	{
		IDXGISwapChain1** SwapChain{};
		ID3D12CommandQueue** CommandQueue{};
		PVOID WndProc{};
		std::uint32_t* ScreenResX{};
		std::uint32_t* ScreenResY{};
		const char* GameVersion{};
		const char* OnlineVersion{};
		rage::atArray<rage::scrThread*>* ScriptThreads{};
		PVOID InitNativeTables{};
		std::int64_t** ScriptGlobals{};
		PVOID RunScriptThreads{};
		Functions::HandleToPtr HandleToPtr{};
		Functions::PtrToHandle PtrToHandle{};
		CPedFactory** PedFactory{};
		bool* IsSessionStarted{};
		rage::scrProgram** ScriptPrograms{};
		Functions::ScriptVM ScriptVM{};
		PVOID PrepareMetricForSending{};
		int* BERestartStatus{};
		bool* NeedsBERestart{};
		bool* IsBEBanned{};
		PVOID HttpStartRequest{};
		PVOID BattlEyeServerProcessPlayerJoin{};
		rage::Obf32** AnticheatInitializedHash{};
		PVOID GetAnticheatInitializedHash{};
		PVOID GetAnticheatInitializedHash2{};
		CAnticheatContext** AnticheatContext{};
		PVOID GameSkeletonUpdate{};
		Functions::QueuePacket QueuePacket{};
		CNetworkPlayerMgr** NetworkPlayerMgr{};
		PVOID ReceiveNetMessage{};
	};

	struct Pointers : PointerData
	{
		bool Init();
		bool LateInit();
		void Restore() {}
	};

	inline YimMenu::Pointers Pointers;
}
