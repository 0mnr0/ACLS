#include "Self.hpp"
#include "core/dma/IMemory.hpp"
#include "game/pointers/Pointers.hpp"

namespace YimMenu
{
	void Self::UpdateImpl()
	{
		if (!Pointers.Initialized || !g_Memory || !Pointers.PedFactory)
		{
			ResetImpl();
			return;
		}

		const auto pedFactory = g_Memory->Read<std::uintptr_t>(Pointers.PedFactory).value_or(0);
		if (!IMemory::IsValidUserAddress(pedFactory))
		{
			ResetImpl();
			return;
		}

		// CPedFactory::m_LocalPed is at offset 0x8 (see types/ped/CPedFactory.hpp)
		const auto localPed = g_Memory->Read<std::uintptr_t>(pedFactory + 0x8).value_or(0);
		if (!IMemory::IsValidUserAddress(localPed))
		{
			ResetImpl();
			return;
		}

		m_LocalPed = localPed;

		if (m_PedVehicleOffset != 0)
		{
			const auto vehicle = g_Memory->Read<std::uintptr_t>(localPed + m_PedVehicleOffset).value_or(0);
			m_Vehicle = IMemory::IsValidUserAddress(vehicle) ? vehicle : 0;
		}
		else
		{
			m_Vehicle = 0;
		}

		if (Pointers.NetworkPlayerMgr && g_Memory)
		{
			const auto netPlayerMgr = g_Memory->Read<std::uintptr_t>(Pointers.NetworkPlayerMgr).value_or(0);
			if (IMemory::IsValidUserAddress(netPlayerMgr))
			{
				std::uint8_t foundId = 0xFF;

				// 1. Direct read via m_LocalPlayer (offset 0xF0) -> m_PlayerIndex (offset 0x61 in GTA5_Enhanced)
				const auto localPlayer = g_Memory->Read<std::uintptr_t>(netPlayerMgr + 0xF0).value_or(0);
				if (IMemory::IsValidUserAddress(localPlayer))
				{
					const auto id = g_Memory->Read<std::uint8_t>(localPlayer + 0x61).value_or(0xFF);
					if (id < 32)
						foundId = id;
				}

				// 2. Fallback: match via m_Players[32] array at offset 0x188
				if (foundId == 0xFF || foundId == 0)
				{
					const auto myPed = m_LocalPed;
					for (std::uint8_t i = 0; i < 32; ++i)
					{
						const auto p = g_Memory->Read<std::uintptr_t>(netPlayerMgr + 0x188 + (i * sizeof(std::uintptr_t))).value_or(0);
						if (!IMemory::IsValidUserAddress(p))
							continue;

						// Check IsLocal flag (m_Flags at offset 0xD0, bit 0)
						const auto flags = g_Memory->Read<std::uint8_t>(p + 0xD0).value_or(0);
						if ((flags & 1) != 0)
						{
							foundId = i;
							break;
						}

						// Check matching ped via CPlayerInfo (CNetGamePlayer::m_PlayerInfo at offset 0xE8)
						if (myPed != 0)
						{
							const auto playerInfo = g_Memory->Read<std::uintptr_t>(p + 0xE8).value_or(0);
							if (IMemory::IsValidUserAddress(playerInfo))
							{
								const auto ped1 = g_Memory->Read<std::uintptr_t>(playerInfo + 0x1E8).value_or(0);
								const auto ped2 = g_Memory->Read<std::uintptr_t>(playerInfo + 0x1F8).value_or(0);
								if (ped1 == myPed || ped2 == myPed)
								{
									foundId = i;
									break;
								}
							}
						}
					}
				}

				m_PlayerId = (foundId < 32) ? foundId : 0;
			}
			else
			{
				m_PlayerId = 0;
			}
		}
		else
		{
			m_PlayerId = 0;
		}
	}

	void Self::ResetImpl()
	{
		m_LocalPed = 0;
		m_Vehicle = 0;
		m_PlayerId = 0;
	}
}
