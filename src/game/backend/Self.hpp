#pragma once
#include <cstdint>

namespace YimMenu
{
	class Self
	{
		std::uintptr_t m_LocalPed{0};
		std::uintptr_t m_Vehicle{0};
		std::uintptr_t m_PedVehicleOffset{0xD10};
		std::uint8_t m_PlayerId{0};

	public:
		static std::uintptr_t GetPedPtr()
		{
			return GetInstance().m_LocalPed;
		}

		static std::uintptr_t GetVehiclePtr()
		{
			return GetInstance().m_Vehicle;
		}

		static std::uint8_t GetPlayerId()
		{
			return GetInstance().m_PlayerId;
		}

		static void SetVehicleOffset(std::uintptr_t offset)
		{
			GetInstance().m_PedVehicleOffset = offset;
		}

		static std::uintptr_t GetVehicleOffset()
		{
			return GetInstance().m_PedVehicleOffset;
		}

		static void Update()
		{
			GetInstance().UpdateImpl();
		}

		static void Reset()
		{
			GetInstance().ResetImpl();
		}

	private:
		Self() = default;
		void UpdateImpl();
		void ResetImpl();

		static Self& GetInstance()
		{
			static Self Instance;
			return Instance;
		}
	};
}
