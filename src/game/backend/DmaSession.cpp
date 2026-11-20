#include "DmaSession.hpp"
#include "core/backend/FiberPool.hpp"
#include "core/backend/ScriptMgr.hpp"
#include "core/commands/BoolCommand.hpp"
#include "core/commands/Command.hpp"
#include "core/commands/Commands.hpp"
#include "core/commands/HotkeySystem.hpp"
#include "core/commands/IntCommand.hpp"
#include "core/commands/StringCommand.hpp"
#include "core/dma/IMemory.hpp"
#include "core/memory/ModuleMgr.hpp"
#include "game/backend/ScriptPatches.hpp"
#include "game/backend/ScriptPointers.hpp"
#include "game/backend/Self.hpp"
#include "game/pointers/Pointers.hpp"

namespace YimMenu
{
	static StringCommand _DmaDevice{
	    "dmadevice",
	    "DMA Device",
	    "MemProcFS -device argument (e.g. fpga, fpga://algo=0, pcie)",
	    "fpga"};

	static StringCommand _DmaProcess{
	    "dmaprocess",
	    "Target Process",
	    "Target game executable name",
	    "GTA5_Enhanced.exe"};

	static BoolCommand _DmaAutoConnect{
	    "dmaautoconnect",
	    "Auto Connect",
	    "Automatically connect to DMA device and retry when disconnected",
	    true};

	static IntCommand _DmaVehicleOffset{
	    "dmavehoffset",
	    "Ped->Vehicle Offset",
	    "CPed::m_Vehicle offset in bytes (default 3344 / 0xD10 for GTA5_Enhanced)",
	    0,
	    0x10000,
	    0xD10};

	class DmaConnectCommand : public Command
	{
		using Command::Command;
		virtual void OnCall() override
		{
			DmaSession::RequestConnect();
		}
	};

	class DmaDisconnectCommand : public Command
	{
		using Command::Command;
		virtual void OnCall() override
		{
			DmaSession::RequestDisconnect();
		}
	};

	static DmaConnectCommand _DmaConnect{"dmaconnect", "Connect / Reconnect", "Connect or reconnect to the DMA device and scan pointers"};
	static DmaDisconnectCommand _DmaDisconnect{"dmadisconnect", "Disconnect", "Disconnect from the DMA device"};

	DmaSession::~DmaSession()
	{
		ShutdownImpl();
	}

	std::string DmaSession::GetStatusMessage()
	{
		auto& inst = GetInstance();
		std::lock_guard lock(inst.m_StatusMutex);
		return inst.m_StatusMessage;
	}

	std::uint32_t DmaSession::GetPid()
	{
		auto& inst = GetInstance();
		std::lock_guard lock(inst.m_StatusMutex);
		return inst.m_Pid;
	}

	std::string DmaSession::GetGameVersion()
	{
		auto& inst = GetInstance();
		std::lock_guard lock(inst.m_StatusMutex);
		return inst.m_GameVersion;
	}

	std::string DmaSession::GetOnlineVersion()
	{
		auto& inst = GetInstance();
		std::lock_guard lock(inst.m_StatusMutex);
		return inst.m_OnlineVersion;
	}

	void DmaSession::SetStatus(DmaState state, std::string message)
	{
		m_State.store(state);
		std::lock_guard lock(m_StatusMutex);
		m_StatusMessage = std::move(message);
	}

	void DmaSession::InitImpl()
	{
		if (m_Running.exchange(true))
			return;

		if (_DmaAutoConnect.GetState())
			m_ConnectRequested.store(true);

		m_WorkerThread = std::thread(&DmaSession::WorkerLoop, this);
	}

	void DmaSession::ShutdownImpl()
	{
		if (!m_Running.exchange(false))
			return;

		if (m_WorkerThread.joinable())
			m_WorkerThread.join();

		DisconnectNow(DmaState::Disconnected, "Disconnected");
	}

	void DmaSession::RequestConnectImpl()
	{
		m_DisconnectRequested.store(false);
		m_ConnectRequested.store(true);
	}

	void DmaSession::RequestDisconnectImpl()
	{
		m_ConnectRequested.store(false);
		m_DisconnectRequested.store(true);
	}

	void DmaSession::DisconnectNow(DmaState newState, std::string message)
	{
		ScriptPatches::Reset();
		Self::Reset();
		Pointers.Reset();

		if (g_Memory)
			g_Memory->Shutdown();

		{
			std::lock_guard lock(m_StatusMutex);
			m_Pid = 0;
			m_GameVersion.clear();
			m_OnlineVersion.clear();
		}

		SetStatus(newState, std::move(message));
	}

	void DmaSession::ConnectNow()
	{
		if (!g_Memory)
		{
			SetStatus(DmaState::Error, "Memory backend is not initialized");
			return;
		}

		const auto deviceArg = _DmaDevice.GetString();
		const auto processName = _DmaProcess.GetString();

		DisconnectNow(DmaState::Connecting, "Initializing MemProcFS (" + deviceArg + ")...");

		if (!g_Memory->Initialize(deviceArg, processName))
		{
			std::string err(g_Memory->GetLastError());
			if (err.empty())
				err = "Failed to initialize MemProcFS / attach to process";
			DisconnectNow(DmaState::Error, std::move(err));
			return;
		}

		SetStatus(DmaState::Connecting, "Enumerating remote modules...");
		if (!ModuleMgr.LoadModules())
		{
			DisconnectNow(DmaState::Error, "Failed to enumerate modules in " + processName);
			return;
		}

		ScriptPointers::Init();

		SetStatus(DmaState::Connecting, "Scanning remote pointers...");
		if (!Pointers.Init())
		{
			DisconnectNow(DmaState::Error, "Pattern scan failed for " + processName);
			return;
		}

		{
			std::lock_guard lock(m_StatusMutex);
			m_Pid = g_Memory->GetPid();
			m_GameVersion = Pointers.GameVersion;
			m_OnlineVersion = Pointers.OnlineVersion;
		}

		SetStatus(DmaState::Connected, "Connected to " + processName);
		LOG(INFO) << "DMA session connected (PID " << g_Memory->GetPid() << ", Game " << Pointers.GameVersion << ", Online " << Pointers.OnlineVersion << ")";
	}

	void DmaSession::WorkerLoop()
	{
		ScriptMgr::Init();
		FiberPool::Init(5);
		ScriptMgr::AddScript(std::make_unique<Script>(&HotkeySystem::RunScript));

		auto lastRetry = std::chrono::steady_clock::now() - std::chrono::seconds(10);
		auto lastHealthCheck = std::chrono::steady_clock::now();

		while (m_Running.load())
		{
			ScriptMgr::Tick();

			if (m_DisconnectRequested.exchange(false))
			{
				DisconnectNow(DmaState::Disconnected, "Disconnected by user");
			}

			if (m_ConnectRequested.exchange(false))
			{
				lastRetry = std::chrono::steady_clock::now();
				ConnectNow();
				lastHealthCheck = std::chrono::steady_clock::now();
			}

			if (m_State.load() == DmaState::Connected)
			{
				const auto now = std::chrono::steady_clock::now();
				if (now - lastHealthCheck >= std::chrono::seconds(2))
				{
					lastHealthCheck = now;
					if (!g_Memory || !g_Memory->IsConnected() || !g_Memory->IsProcessAttached())
					{
						const auto processName = _DmaProcess.GetString();
						if (!g_Memory || !g_Memory->AttachProcess(processName))
						{
							LOG(WARNING) << "Lost connection to DMA target process " << processName;
							DisconnectNow(DmaState::Disconnected, "Target process exited or DMA disconnected");
							continue;
						}
					}
					else if (Pointers.PedFactory && !g_Memory->Read<std::uintptr_t>(Pointers.PedFactory).has_value())
					{
						const auto processName = _DmaProcess.GetString();
						if (!g_Memory->AttachProcess(processName))
						{
							LOG(WARNING) << "Lost connection to DMA target process " << processName;
							DisconnectNow(DmaState::Disconnected, "Target process exited or DMA disconnected");
							continue;
						}
					}
				}

				Self::SetVehicleOffset(static_cast<std::uintptr_t>(_DmaVehicleOffset.GetState()));
				Self::Update();
				Commands::RunLoopedCommands();
				ScriptPatches::Tick();

				std::this_thread::sleep_for(std::chrono::milliseconds(16));
			}
			else
			{
				if (_DmaAutoConnect.GetState() && !m_DisconnectRequested.load())
				{
					const auto now = std::chrono::steady_clock::now();
					if (now - lastRetry >= std::chrono::seconds(3))
					{
						lastRetry = now;
						ConnectNow();
						lastHealthCheck = std::chrono::steady_clock::now();
						continue;
					}
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}

		FiberPool::Destroy();
		ScriptMgr::Destroy();
	}
}
