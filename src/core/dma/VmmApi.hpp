#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>

namespace YimMenu::Vmm
{
	using VMM_HANDLE = void*;
	using VMMDLL_SCATTER_HANDLE = HANDLE;
	using QWORD = unsigned __int64;

	inline constexpr ULONG64 FLAG_NOCACHE = 0x0001;
	inline constexpr ULONG64 FLAG_NOCACHEPUT = 0x0100;
	inline constexpr DWORD MODULE_FLAG_NORMAL = 0;

	enum VMMDLL_MODULE_TP : DWORD
	{
		VMMDLL_MODULE_TP_NORMAL = 0,
		VMMDLL_MODULE_TP_DATA = 1,
		VMMDLL_MODULE_TP_NOTLINKED = 2,
		VMMDLL_MODULE_TP_INJECTED = 3,
	};

	struct VMMDLL_MAP_MODULEENTRY
	{
		QWORD vaBase;
		QWORD vaEntry;
		DWORD cbImageSize;
		BOOL fWoW64;
		union
		{
			LPSTR uszText;
			LPWSTR wszText;
		};
		DWORD _Reserved3;
		DWORD _Reserved4;
		union
		{
			LPSTR uszFullName;
			LPWSTR wszFullName;
		};
		VMMDLL_MODULE_TP tp;
		DWORD cbFileSizeRaw;
		DWORD cSection;
		DWORD cEAT;
		DWORD cIAT;
		DWORD _Reserved2;
		QWORD _Reserved1[3];
		void* pExDebugInfo;
		void* pExVersionInfo;
	};

	struct VMMDLL_MAP_MODULE
	{
		DWORD dwVersion;
		DWORD _Reserved1[5];
		PBYTE pbMultiText;
		DWORD cbMultiText;
		DWORD cMap;
		VMMDLL_MAP_MODULEENTRY pMap[1];
	};

	struct VmmApi
	{
		using FnInitialize = VMM_HANDLE (*)(DWORD argc, LPCSTR argv[]);
		using FnClose = VOID (*)(VMM_HANDLE hVMM);
		using FnMemFree = VOID (*)(PVOID pvMem);
		using FnPidGetFromName = BOOL (*)(VMM_HANDLE hVMM, LPCSTR szProcName, PDWORD pdwPID);
		using FnMapGetModuleU = BOOL (*)(VMM_HANDLE hVMM, DWORD dwPID, VMMDLL_MAP_MODULE** ppModuleMap, DWORD flags);
		using FnMapGetModuleFromNameU = BOOL (*)(VMM_HANDLE hVMM, DWORD dwPID, LPCSTR uszModuleName, VMMDLL_MAP_MODULEENTRY** ppModuleMapEntry, DWORD flags);
		using FnMemReadEx = BOOL (*)(VMM_HANDLE hVMM, DWORD dwPID, ULONG64 qwA, PBYTE pb, DWORD cb, PDWORD pcbReadOpt, ULONG64 flags);
		using FnMemWrite = BOOL (*)(VMM_HANDLE hVMM, DWORD dwPID, ULONG64 qwA, PBYTE pb, DWORD cb);
		using FnScatterInitialize = VMMDLL_SCATTER_HANDLE (*)(VMM_HANDLE hVMM, DWORD dwPID, DWORD flags);
		using FnScatterPrepare = BOOL (*)(VMMDLL_SCATTER_HANDLE hS, QWORD va, DWORD cb);
		using FnScatterPrepareEx = BOOL (*)(VMMDLL_SCATTER_HANDLE hS, QWORD va, DWORD cb, PBYTE pb, PDWORD pcbRead);
		using FnScatterPrepareWrite = BOOL (*)(VMMDLL_SCATTER_HANDLE hS, QWORD va, PBYTE pb, DWORD cb);
		using FnScatterExecute = BOOL (*)(VMMDLL_SCATTER_HANDLE hS);
		using FnScatterExecuteRead = BOOL (*)(VMMDLL_SCATTER_HANDLE hS);
		using FnScatterRead = BOOL (*)(VMMDLL_SCATTER_HANDLE hS, QWORD va, DWORD cb, PBYTE pb, PDWORD pcbRead);
		using FnScatterClear = BOOL (*)(VMMDLL_SCATTER_HANDLE hS, DWORD dwPID, DWORD flags);
		using FnScatterCloseHandle = VOID (*)(VMMDLL_SCATTER_HANDLE hS);

		HMODULE module{nullptr};
		FnInitialize Initialize{nullptr};
		FnClose Close{nullptr};
		FnMemFree MemFree{nullptr};
		FnPidGetFromName PidGetFromName{nullptr};
		FnMapGetModuleU MapGetModuleU{nullptr};
		FnMapGetModuleFromNameU MapGetModuleFromNameU{nullptr};
		FnMemReadEx MemReadEx{nullptr};
		FnMemWrite MemWrite{nullptr};
		FnScatterInitialize ScatterInitialize{nullptr};
		FnScatterPrepare ScatterPrepare{nullptr};
		FnScatterPrepareEx ScatterPrepareEx{nullptr};
		FnScatterPrepareWrite ScatterPrepareWrite{nullptr};
		FnScatterExecute ScatterExecute{nullptr};
		FnScatterExecuteRead ScatterExecuteRead{nullptr};
		FnScatterRead ScatterRead{nullptr};
		FnScatterClear ScatterClear{nullptr};
		FnScatterCloseHandle ScatterCloseHandle{nullptr};

		bool Load(std::string& outError)
		{
			if (module)
				return true;

			module = LoadLibraryW(L"vmm.dll");
			if (!module)
			{
				outError = "Failed to load vmm.dll (place vmm.dll, leechcore.dll, and FTD3XX.dll next to the executable)";
				return false;
			}

			auto resolve = [&](auto& fn, const char* name) -> bool {
				fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(GetProcAddress(module, name));
				if (!fn)
				{
					outError = std::string("Missing export in vmm.dll: ") + name;
					return false;
				}
				return true;
			};

			bool ok = resolve(Initialize, "VMMDLL_Initialize")
			       && resolve(Close, "VMMDLL_Close")
			       && resolve(MemFree, "VMMDLL_MemFree")
			       && resolve(PidGetFromName, "VMMDLL_PidGetFromName")
			       && resolve(MapGetModuleU, "VMMDLL_Map_GetModuleU")
			       && resolve(MapGetModuleFromNameU, "VMMDLL_Map_GetModuleFromNameU")
			       && resolve(MemReadEx, "VMMDLL_MemReadEx")
			       && resolve(MemWrite, "VMMDLL_MemWrite")
			       && resolve(ScatterInitialize, "VMMDLL_Scatter_Initialize")
			       && resolve(ScatterPrepare, "VMMDLL_Scatter_Prepare")
			       && resolve(ScatterPrepareEx, "VMMDLL_Scatter_PrepareEx")
			       && resolve(ScatterPrepareWrite, "VMMDLL_Scatter_PrepareWrite")
			       && resolve(ScatterExecute, "VMMDLL_Scatter_Execute")
			       && resolve(ScatterExecuteRead, "VMMDLL_Scatter_ExecuteRead")
			       && resolve(ScatterRead, "VMMDLL_Scatter_Read")
			       && resolve(ScatterClear, "VMMDLL_Scatter_Clear")
			       && resolve(ScatterCloseHandle, "VMMDLL_Scatter_CloseHandle");

			if (!ok)
			{
				Unload();
				return false;
			}

			return true;
		}

		void Unload()
		{
			if (module)
			{
				FreeLibrary(module);
				module = nullptr;
			}
			Initialize = nullptr;
			Close = nullptr;
			MemFree = nullptr;
			PidGetFromName = nullptr;
			MapGetModuleU = nullptr;
			MapGetModuleFromNameU = nullptr;
			MemReadEx = nullptr;
			MemWrite = nullptr;
			ScatterInitialize = nullptr;
			ScatterPrepare = nullptr;
			ScatterPrepareEx = nullptr;
			ScatterPrepareWrite = nullptr;
			ScatterExecute = nullptr;
			ScatterExecuteRead = nullptr;
			ScatterRead = nullptr;
			ScatterClear = nullptr;
			ScatterCloseHandle = nullptr;
		}
	};
}
