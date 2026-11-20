#pragma once
#include "IMemory.hpp"
#include "VmmApi.hpp"
#include <mutex>

namespace YimMenu
{
	class DmaScatterHandle final : public IScatterHandle
	{
	public:
		DmaScatterHandle(Vmm::VmmApi& api, Vmm::VMM_HANDLE vmm, DWORD pid, DWORD flags);
		~DmaScatterHandle() override;

		DmaScatterHandle(const DmaScatterHandle&) = delete;
		DmaScatterHandle& operator=(const DmaScatterHandle&) = delete;

		bool PrepareRead(std::uintptr_t address, void* outBuffer, std::size_t size) override;
		bool PrepareWrite(std::uintptr_t address, const void* data, std::size_t size) override;
		bool Execute() override;
		bool Clear() override;

		bool IsValid() const { return m_Handle != nullptr; }

	private:
		Vmm::VmmApi& m_Api;
		Vmm::VMMDLL_SCATTER_HANDLE m_Handle{nullptr};
		DWORD m_Pid{0};
		DWORD m_Flags{0};
	};

	class DmaMemory final : public IMemory
	{
	public:
		DmaMemory() = default;
		~DmaMemory() override;

		bool Initialize(std::string_view deviceArg, std::string_view processName) override;
		void Shutdown() override;

		bool IsConnected() const override { return m_Vmm != nullptr; }
		bool IsProcessAttached() const override { return m_Vmm != nullptr && m_Pid != 0; }
		bool AttachProcess(std::string_view processName) override;

		std::uint32_t GetPid() const override { return m_Pid; }
		std::string_view GetProcessName() const override { return m_ProcessName; }
		std::string_view GetLastError() const override { return m_LastError; }

		bool ReadBuffer(std::uintptr_t address, void* buffer, std::size_t size, bool noCache = true) override;
		bool WriteBuffer(std::uintptr_t address, const void* buffer, std::size_t size) override;

		bool GetModule(std::string_view moduleName, RemoteModuleInfo& outInfo) override;
		std::vector<RemoteModuleInfo> GetModules() override;
		std::unique_ptr<IScatterHandle> CreateScatterHandle(bool noCache = true) override;

	private:
		Vmm::VmmApi m_Api{};
		Vmm::VMM_HANDLE m_Vmm{nullptr};
		DWORD m_Pid{0};
		std::string m_ProcessName{};
		std::string m_LastError{};
		mutable std::mutex m_Mutex{};
	};
}
