#pragma once

class CBattlEyePlayerModifyInterface;
class CBattlEyePlayerModifyContext;

namespace rage
{
	class netConnectionManager;
	class netEvent;
	class scrProgram;
	class JsonSerializer;
	class rlMetric;
	struct gameSkeleton;
}

namespace YimMenu::Hooks
{
	namespace Anticheat
	{
		extern void GameSkeletonUpdate(rage::gameSkeleton* skeleton, int type);
		extern bool PrepareMetricForSending(rage::JsonSerializer* ser, bool* failed, char* a3, uint64_t time, rage::rlMetric* metric);
		extern BOOL GetThreadContext(HANDLE hThread, LPCONTEXT lpContext);
		extern void HttpStartRequest(void* request);
		extern bool BattlEyeServerProcessPlayerJoin(CBattlEyePlayerModifyInterface* server_iface, CBattlEyePlayerModifyContext* context);
		extern std::uint32_t GetAnticheatInitializedHash();
		extern std::uint32_t GetAnticheatInitializedHash2(void* ac_var, std::uint32_t seed);
	}

	namespace Script
	{
		extern bool RunScriptThreads(int ops_to_execute);
		extern void InitNativeTables(rage::scrProgram* program);
		extern int ScriptVM(uint64_t* stack, uint64_t** scr_globals, rage::scrProgram* program, void* ctx);
	}

	namespace Network
	{
		extern void ReceiveNetMessage(void* a1, rage::netConnectionManager* mgr, rage::netEvent* event);
	}

	namespace Window
	{
		extern LRESULT WndProc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam);
	}

	namespace RawInput
	{
		extern UINT GetRawInputData(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader);
	}
}
