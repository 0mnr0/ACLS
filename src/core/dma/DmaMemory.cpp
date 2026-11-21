#include "DmaMemory.hpp"
#include "common.hpp"
#include <sstream>

namespace YimMenu
{
	DmaScatterHandle::DmaScatterHandle(Vmm::VmmApi& api, Vmm::VMM_HANDLE vmm, DWORD pid, DWORD flags) :
	    m_Api(api),
	    m_Pid(pid),
	    m_Flags(flags)
	{
		if (m_Api.ScatterInitialize && vmm && pid)
		{
			m_Handle = m_Api.ScatterInitialize(vmm, pid, flags);
		}
	}

	DmaScatterHandle::~DmaScatterHandle()
	{
		if (m_Handle && m_Api.ScatterCloseHandle)
		{
			m_Api.ScatterCloseHandle(m_Handle);
			m_Handle = nullptr;
		}
	}

	bool DmaScatterHandle::PrepareRead(std::uintptr_t address, void* outBuffer, std::size_t size)
	{
		if (!m_Handle || !m_Api.ScatterPrepareEx || !outBuffer || size == 0)
			return false;
		if (!IMemory::IsValidUserAddress(address))
			return false;

		return m_Api.ScatterPrepareEx(m_Handle, static_cast<Vmm::QWORD>(address), static_cast<DWORD>(size), static_cast<PBYTE>(outBuffer), nullptr) != FALSE;
	}

	bool DmaScatterHandle::PrepareWrite(std::uintptr_t address, const void* data, std::size_t size)
	{
		if (!m_Handle || !m_Api.ScatterPrepareWrite || !data || size == 0)
			return false;
		if (!IMemory::IsValidUserAddress(address))
			return false;

		return m_Api.ScatterPrepareWrite(m_Handle, static_cast<Vmm::QWORD>(address), const_cast<PBYTE>(static_cast<const BYTE*>(data)), static_cast<DWORD>(size)) != FALSE;
	}

	bool DmaScatterHandle::Execute()
	{
		if (!m_Handle || !m_Api.ScatterExecute)
			return false;

		return m_Api.ScatterExecute(m_Handle) != FALSE;
	}

	bool DmaScatterHandle::Clear()
	{
		if (!m_Handle || !m_Api.ScatterClear)
			return false;

		return m_Api.ScatterClear(m_Handle, m_Pid, m_Flags) != FALSE;
	}

	DmaMemory::~DmaMemory()
	{
		Shutdown();
	}

	bool DmaMemory::Initialize(std::string_view deviceArg, std::string_view processName)
	{
		std::lock_guard lock(m_Mutex);

		if (m_Vmm)
		{
			m_Api.Close(m_Vmm);
			m_Vmm = nullptr;
		}
		m_Pid = 0;
		m_LastError.clear();

		if (!m_Api.Load(m_LastError))
		{
			LOG(WARNING) << m_LastError;
			return false;
		}

		std::vector<std::string> tokens;
		tokens.emplace_back("");
		tokens.emplace_back("-device");

		if (deviceArg.empty())
		{
			tokens.emplace_back("fpga");
		}
		else
		{
			std::istringstream iss{std::string(deviceArg)};
			std::string tok;
			bool first = true;
			while (iss >> tok)
			{
				if (first && tok == "-device")
				{
					first = false;
					continue;
				}
				first = false;
				tokens.push_back(std::move(tok));
			}
			if (tokens.size() == 2)
				tokens.emplace_back("fpga");
		}

		std::vector<LPCSTR> argv;
		argv.reserve(tokens.size());
		for (const auto& t : tokens)
			argv.push_back(t.c_str());

		m_Vmm = m_Api.Initialize(static_cast<DWORD>(argv.size()), argv.data());
		if (!m_Vmm)
		{
			m_LastError = "VMMDLL_Initialize failed (check DMA hardware/connection and device arguments)";
			LOG(WARNING) << m_LastError;
			return false;
		}

		LOGF(INFO, "MemProcFS initialized with device '{}'", tokens[2]);

		if (!processName.empty())
		{
			m_ProcessName = processName;
			DWORD pid = 0;
			if (!m_Api.PidGetFromName(m_Vmm, m_ProcessName.c_str(), &pid) || pid == 0)
			{
				m_LastError = std::string("Process not found: ") + m_ProcessName;
				LOG(INFO) << m_LastError;
				return false;
			}
			m_Pid = pid;
			LOGF(INFO, "Attached to {} (PID: {})", m_ProcessName, m_Pid);
		}

		return true;
	}

	void DmaMemory::Shutdown()
	{
		std::lock_guard lock(m_Mutex);
		m_Pid = 0;
		if (m_Vmm && m_Api.Close)
		{
			m_Api.Close(m_Vmm);
			m_Vmm = nullptr;
		}
		m_Api.Unload();
	}

	bool DmaMemory::AttachProcess(std::string_view processName)
	{
		std::lock_guard lock(m_Mutex);
		if (!m_Vmm)
		{
			m_LastError = "DMA device not initialized";
			return false;
		}

		m_ProcessName = processName;
		DWORD pid = 0;
		if (!m_Api.PidGetFromName(m_Vmm, m_ProcessName.c_str(), &pid) || pid == 0)
		{
			m_Pid = 0;
			m_LastError = std::string("Waiting for process: ") + m_ProcessName;
			return false;
		}

		if (m_Pid == pid)
		{
			m_LastError.clear();
			return true;
		}

		m_Pid = pid;
		m_LastError.clear();
		LOGF(INFO, "Attached to {} (PID: {})", m_ProcessName, m_Pid);
		return true;
	}

	bool DmaMemory::ReadBuffer(std::uintptr_t address, void* buffer, std::size_t size, bool noCache)
	{
		if (!m_Vmm || !m_Pid || !buffer || size == 0)
			return false;

		constexpr std::size_t maxChunk = 0x100000; // 1 MB per VMMDLL_MemReadEx call
		auto* dst = static_cast<BYTE*>(buffer);
		std::size_t offset = 0;
		const ULONG64 flags = noCache ? Vmm::FLAG_NOCACHE : 0;

		while (offset < size)
		{
			const DWORD chunk = static_cast<DWORD>(std::min(maxChunk, size - offset));
			DWORD bytesRead = 0;
			if (!m_Api.MemReadEx(m_Vmm, m_Pid, static_cast<ULONG64>(address + offset), dst + offset, chunk, &bytesRead, flags))
				return false;
			if (bytesRead == 0)
				return false;
			offset += chunk;
		}

		return true;
	}

	bool DmaMemory::WriteBuffer(std::uintptr_t address, const void* buffer, std::size_t size)
	{
		if (!m_Vmm || !m_Pid || !buffer || size == 0)
			return false;

		return m_Api.MemWrite(m_Vmm, m_Pid, static_cast<ULONG64>(address), const_cast<PBYTE>(static_cast<const BYTE*>(buffer)), static_cast<DWORD>(size)) != FALSE;
	}

	bool DmaMemory::GetModule(std::string_view moduleName, RemoteModuleInfo& outInfo)
	{
		if (!m_Vmm || !m_Pid)
			return false;

		std::string nameStr(moduleName);
		Vmm::VMMDLL_MAP_MODULEENTRY* entry = nullptr;
		if (!m_Api.MapGetModuleFromNameU(m_Vmm, m_Pid, nameStr.c_str(), &entry, Vmm::MODULE_FLAG_NORMAL) || !entry)
			return false;

		outInfo.name = entry->uszText ? entry->uszText : nameStr;
		outInfo.fullPath = entry->uszFullName ? entry->uszFullName : outInfo.name;
		outInfo.base = static_cast<std::uintptr_t>(entry->vaBase);
		outInfo.size = static_cast<std::uintptr_t>(entry->cbImageSize);
		m_Api.MemFree(entry);
		return outInfo.base != 0 && outInfo.size != 0;
	}

	std::vector<RemoteModuleInfo> DmaMemory::GetModules()
	{
		std::vector<RemoteModuleInfo> result;
		if (!m_Vmm || !m_Pid)
			return result;

		Vmm::VMMDLL_MAP_MODULE* moduleMap = nullptr;
		if (!m_Api.MapGetModuleU(m_Vmm, m_Pid, &moduleMap, Vmm::MODULE_FLAG_NORMAL) || !moduleMap)
			return result;

		result.reserve(moduleMap->cMap);
		for (DWORD i = 0; i < moduleMap->cMap; ++i)
		{
			const auto& e = moduleMap->pMap[i];
			RemoteModuleInfo info{};
			info.name = e.uszText ? e.uszText : "";
			info.fullPath = e.uszFullName ? e.uszFullName : info.name;
			info.base = static_cast<std::uintptr_t>(e.vaBase);
			info.size = static_cast<std::uintptr_t>(e.cbImageSize);
			if (info.base && info.size && !info.name.empty())
				result.push_back(std::move(info));
		}

		m_Api.MemFree(moduleMap);
		return result;
	}

	std::unique_ptr<IScatterHandle> DmaMemory::CreateScatterHandle(bool noCache)
	{
		if (!m_Vmm || !m_Pid)
			return nullptr;
		const DWORD flags = noCache ? static_cast<DWORD>(Vmm::FLAG_NOCACHE) : 0;
		auto handle = std::make_unique<DmaScatterHandle>(m_Api, m_Vmm, m_Pid, flags);
		if (!handle->IsValid())
			return nullptr;
		return handle;
	}
}
