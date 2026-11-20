#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace YimMenu
{
	enum class DmaState
	{
		Disconnected,
		Connecting,
		Connected,
		Error
	};

	class DmaSession
	{
	public:
		static void Init()
		{
			GetInstance().InitImpl();
		}

		static void Shutdown()
		{
			GetInstance().ShutdownImpl();
		}

		static void RequestConnect()
		{
			GetInstance().RequestConnectImpl();
		}

		static void RequestDisconnect()
		{
			GetInstance().RequestDisconnectImpl();
		}

		static DmaState GetState()
		{
			return GetInstance().m_State.load();
		}

		static std::string GetStatusMessage();
		static std::uint32_t GetPid();
		static std::string GetGameVersion();
		static std::string GetOnlineVersion();

	private:
		DmaSession() = default;
		~DmaSession();

		static DmaSession& GetInstance()
		{
			static DmaSession Instance;
			return Instance;
		}

		void InitImpl();
		void ShutdownImpl();
		void RequestConnectImpl();
		void RequestDisconnectImpl();

		void WorkerLoop();
		void ConnectNow();
		void DisconnectNow(DmaState newState, std::string message);
		void SetStatus(DmaState state, std::string message);

		std::atomic<bool> m_Running{false};
		std::atomic<bool> m_ConnectRequested{false};
		std::atomic<bool> m_DisconnectRequested{false};
		std::atomic<DmaState> m_State{DmaState::Disconnected};

		std::mutex m_StatusMutex;
		std::string m_StatusMessage{"Waiting to connect"};
		std::uint32_t m_Pid{0};
		std::string m_GameVersion{};
		std::string m_OnlineVersion{};

		std::thread m_WorkerThread;
	};
}
