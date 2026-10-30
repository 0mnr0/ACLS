#include "core/commands/IntCommand.hpp"
#include "core/commands/LoopedCommand.hpp"
#include "game/backend/Self.hpp"
#include "game/pointers/Pointers.hpp"

namespace YimMenu::Features
{
	static IntCommand _GhostModeAlpha{"ghostmodealpha", "Ghost Alpha", "Alpha transparency of the vehicle in ghost mode (0 = invisible, 255 = opaque)", 0, 255, 105};

	struct OriginalVehicleValues
	{
		uint8_t alpha{0xFF};
		uint8_t ghost{0};
		uint32_t col_mask{0};
		uint8_t bit_94b{0};
		uint8_t bit_980{0x04};
		uint8_t bit_987{0};
		uint8_t bit_1a47{0};
		bool valid{false};
	};

	class GhostMode : public LoopedCommand
	{
		using LoopedCommand::LoopedCommand;

		OriginalVehicleValues m_Originals{};
		uint8_t* m_LastVehiclePointer{nullptr};

		void SaveOriginals(uint8_t* ptr)
		{
			if (!ptr)
				return;

			m_Originals.alpha = *(uint8_t*)(ptr + 0xAC);
			m_Originals.ghost = *(uint8_t*)(ptr + 0x2CE);
			m_Originals.col_mask = *(uint32_t*)(ptr + 0x708);
			m_Originals.bit_94b = *(uint8_t*)(ptr + 0x94B);
			m_Originals.bit_980 = *(uint8_t*)(ptr + 0x980);
			m_Originals.bit_987 = *(uint8_t*)(ptr + 0x987);
			m_Originals.bit_1a47 = *(uint8_t*)(ptr + 0x1A47);
			m_Originals.valid = true;
		}

		void Restore(uint8_t* ptr)
		{
			if (!ptr)
				return;

			if (m_Originals.valid)
			{
				*(uint8_t*)(ptr + 0xAC) = m_Originals.alpha;
				*(uint8_t*)(ptr + 0x2CE) = m_Originals.ghost;
				*(uint32_t*)(ptr + 0x708) = m_Originals.col_mask;
				*(uint8_t*)(ptr + 0x94B) = m_Originals.bit_94b;
				*(uint8_t*)(ptr + 0x980) = m_Originals.bit_980;
				*(uint8_t*)(ptr + 0x987) = m_Originals.bit_987;
				*(uint8_t*)(ptr + 0x1A47) = m_Originals.bit_1a47;
			}
			else
			{
				*(uint8_t*)(ptr + 0xAC) = 0xFF;
				*(uint8_t*)(ptr + 0x2CE) = 0;
				*(uint32_t*)(ptr + 0x708) = 0;
				*(uint8_t*)(ptr + 0x94B) &= ~0x80;
				*(uint8_t*)(ptr + 0x980) |= 0x04;
				*(uint8_t*)(ptr + 0x987) &= ~0x02;
				*(uint8_t*)(ptr + 0x1A47) &= ~0x80;
			}
			m_Originals.valid = false;
		}

		void Apply(uint8_t* ptr)
		{
			if (!ptr)
				return;

			*(uint8_t*)(ptr + 0xAC) = static_cast<uint8_t>(std::clamp(_GhostModeAlpha.GetState(), 0, 255));
			*(uint8_t*)(ptr + 0x2CE) = 1;
			*(uint32_t*)(ptr + 0x708) = 0xFFFFFFFF;
			*(uint8_t*)(ptr + 0x94B) |= 0x80;
			*(uint8_t*)(ptr + 0x980) &= ~0x04;
			*(uint8_t*)(ptr + 0x987) |= 0x02;
			*(uint8_t*)(ptr + 0x1A47) |= 0x80;
		}

		virtual void OnTick() override
		{
			auto veh = Self::GetVehicle();
			uint8_t* ptr = (veh && veh.IsValid()) ? veh.GetPointer<uint8_t*>() : nullptr;

			if (ptr != m_LastVehiclePointer)
			{
				if (m_LastVehiclePointer && Pointers.PtrToHandle(m_LastVehiclePointer) != 0)
				{
					Restore(m_LastVehiclePointer);
				}
				m_LastVehiclePointer = ptr;
				if (ptr)
				{
					SaveOriginals(ptr);
				}
				else
				{
					m_Originals.valid = false;
				}
			}

			if (ptr)
			{
				Apply(ptr);
			}
		}

		virtual void OnDisable() override
		{
			if (m_LastVehiclePointer)
			{
				if (Pointers.PtrToHandle(m_LastVehiclePointer) != 0)
				{
					Restore(m_LastVehiclePointer);
				}
				m_LastVehiclePointer = nullptr;
				m_Originals.valid = false;
			}
			else if (auto veh = Self::GetVehicle(); veh && veh.IsValid())
			{
				if (auto ptr = veh.GetPointer<uint8_t*>())
				{
					Restore(ptr);
				}
			}
		}
	};

	static GhostMode _GhostMode{"ghostmode", "Ghost Mode", "Pass through vehicles and objects with transparency"};
}
