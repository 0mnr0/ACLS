#include "core/commands/IntCommand.hpp"
#include "core/commands/LoopedCommand.hpp"
#include "core/dma/IMemory.hpp"
#include "game/backend/Self.hpp"
#include "game/pointers/Pointers.hpp"

namespace YimMenu::Features
{
	static IntCommand _GhostModeAlpha{"ghostmodealpha", "Ghost Alpha", "Alpha transparency of the vehicle in ghost mode (0 = invisible, 255 = opaque)", 0, 255, 105};

	struct OriginalVehicleValues
	{
		std::uint8_t alpha{0xFF};
		std::uint8_t ghost{0};
		std::uint32_t col_mask{0};
		std::uint8_t bit_94b{0};
		std::uint8_t bit_980{0x04};
		std::uint8_t bit_987{0};
		std::uint8_t bit_1a47{0};
		bool valid{false};
	};

	class GhostMode : public LoopedCommand
	{
		using LoopedCommand::LoopedCommand;

		OriginalVehicleValues m_Originals{};
		std::uintptr_t m_LastVehiclePointer{0};

		bool IsRemoteVehicleReadable(std::uintptr_t ptr)
		{
			return g_Memory && IMemory::IsValidUserAddress(ptr) && g_Memory->Read<std::uint8_t>(ptr + 0xAC).has_value();
		}

		void SaveOriginals(std::uintptr_t ptr)
		{
			if (!g_Memory || !IMemory::IsValidUserAddress(ptr))
				return;

			OriginalVehicleValues vals{};
			if (auto scatter = g_Memory->CreateScatterHandle())
			{
				scatter->PrepareRead(ptr + 0xAC, vals.alpha);
				scatter->PrepareRead(ptr + 0x2CE, vals.ghost);
				scatter->PrepareRead(ptr + 0x708, vals.col_mask);
				scatter->PrepareRead(ptr + 0x94B, vals.bit_94b);
				scatter->PrepareRead(ptr + 0x980, vals.bit_980);
				scatter->PrepareRead(ptr + 0x987, vals.bit_987);
				scatter->PrepareRead(ptr + 0x1A47, vals.bit_1a47);
				if (scatter->Execute())
				{
					vals.valid = true;
					m_Originals = vals;
					return;
				}
			}

			auto alpha = g_Memory->Read<std::uint8_t>(ptr + 0xAC);
			auto ghost = g_Memory->Read<std::uint8_t>(ptr + 0x2CE);
			auto colMask = g_Memory->Read<std::uint32_t>(ptr + 0x708);
			auto bit94b = g_Memory->Read<std::uint8_t>(ptr + 0x94B);
			auto bit980 = g_Memory->Read<std::uint8_t>(ptr + 0x980);
			auto bit987 = g_Memory->Read<std::uint8_t>(ptr + 0x987);
			auto bit1a47 = g_Memory->Read<std::uint8_t>(ptr + 0x1A47);

			if (alpha && ghost && colMask && bit94b && bit980 && bit987 && bit1a47)
			{
				m_Originals.alpha = *alpha;
				m_Originals.ghost = *ghost;
				m_Originals.col_mask = *colMask;
				m_Originals.bit_94b = *bit94b;
				m_Originals.bit_980 = *bit980;
				m_Originals.bit_987 = *bit987;
				m_Originals.bit_1a47 = *bit1a47;
				m_Originals.valid = true;
			}
		}

		void Restore(std::uintptr_t ptr)
		{
			if (!g_Memory || !IsRemoteVehicleReadable(ptr))
				return;

			if (m_Originals.valid)
			{
				if (auto scatter = g_Memory->CreateScatterHandle())
				{
					scatter->PrepareWrite(ptr + 0xAC, m_Originals.alpha);
					scatter->PrepareWrite(ptr + 0x2CE, m_Originals.ghost);
					scatter->PrepareWrite(ptr + 0x708, m_Originals.col_mask);
					scatter->PrepareWrite(ptr + 0x94B, m_Originals.bit_94b);
					scatter->PrepareWrite(ptr + 0x980, m_Originals.bit_980);
					scatter->PrepareWrite(ptr + 0x987, m_Originals.bit_987);
					scatter->PrepareWrite(ptr + 0x1A47, m_Originals.bit_1a47);
					scatter->Execute();
				}
				else
				{
					g_Memory->Write(ptr + 0xAC, m_Originals.alpha);
					g_Memory->Write(ptr + 0x2CE, m_Originals.ghost);
					g_Memory->Write(ptr + 0x708, m_Originals.col_mask);
					g_Memory->Write(ptr + 0x94B, m_Originals.bit_94b);
					g_Memory->Write(ptr + 0x980, m_Originals.bit_980);
					g_Memory->Write(ptr + 0x987, m_Originals.bit_987);
					g_Memory->Write(ptr + 0x1A47, m_Originals.bit_1a47);
				}
			}
			else
			{
				auto bit94b = g_Memory->Read<std::uint8_t>(ptr + 0x94B).value_or(0);
				auto bit980 = g_Memory->Read<std::uint8_t>(ptr + 0x980).value_or(0);
				auto bit987 = g_Memory->Read<std::uint8_t>(ptr + 0x987).value_or(0);
				auto bit1a47 = g_Memory->Read<std::uint8_t>(ptr + 0x1A47).value_or(0);

				const std::uint8_t alpha = 0xFF;
				const std::uint8_t ghost = 0;
				const std::uint32_t colMask = 0;
				bit94b &= ~0x80;
				bit980 |= 0x04;
				bit987 &= ~0x02;
				bit1a47 &= ~0x80;

				g_Memory->Write(ptr + 0xAC, alpha);
				g_Memory->Write(ptr + 0x2CE, ghost);
				g_Memory->Write(ptr + 0x708, colMask);
				g_Memory->Write(ptr + 0x94B, bit94b);
				g_Memory->Write(ptr + 0x980, bit980);
				g_Memory->Write(ptr + 0x987, bit987);
				g_Memory->Write(ptr + 0x1A47, bit1a47);
			}

			m_Originals.valid = false;
		}

		void Apply(std::uintptr_t ptr)
		{
			if (!g_Memory || !IMemory::IsValidUserAddress(ptr))
				return;

			std::uint8_t bit94b = 0;
			std::uint8_t bit980 = 0;
			std::uint8_t bit987 = 0;
			std::uint8_t bit1a47 = 0;

			bool readOk = false;
			if (auto scatter = g_Memory->CreateScatterHandle())
			{
				scatter->PrepareRead(ptr + 0x94B, bit94b);
				scatter->PrepareRead(ptr + 0x980, bit980);
				scatter->PrepareRead(ptr + 0x987, bit987);
				scatter->PrepareRead(ptr + 0x1A47, bit1a47);
				readOk = scatter->Execute();
			}

			if (!readOk)
			{
				auto b1 = g_Memory->Read<std::uint8_t>(ptr + 0x94B);
				auto b2 = g_Memory->Read<std::uint8_t>(ptr + 0x980);
				auto b3 = g_Memory->Read<std::uint8_t>(ptr + 0x987);
				auto b4 = g_Memory->Read<std::uint8_t>(ptr + 0x1A47);
				if (!b1 || !b2 || !b3 || !b4)
					return;
				bit94b = *b1;
				bit980 = *b2;
				bit987 = *b3;
				bit1a47 = *b4;
			}

			const auto alpha = static_cast<std::uint8_t>(std::clamp(_GhostModeAlpha.GetState(), 0, 255));
			const std::uint8_t ghost = 1;
			const std::uint32_t colMask = 0xFFFFFFFF;
			bit94b |= 0x80;
			bit980 &= ~0x04;
			bit987 |= 0x02;
			bit1a47 |= 0x80;

			if (auto scatter = g_Memory->CreateScatterHandle())
			{
				scatter->PrepareWrite(ptr + 0xAC, alpha);
				scatter->PrepareWrite(ptr + 0x2CE, ghost);
				scatter->PrepareWrite(ptr + 0x708, colMask);
				scatter->PrepareWrite(ptr + 0x94B, bit94b);
				scatter->PrepareWrite(ptr + 0x980, bit980);
				scatter->PrepareWrite(ptr + 0x987, bit987);
				scatter->PrepareWrite(ptr + 0x1A47, bit1a47);
				scatter->Execute();
			}
			else
			{
				g_Memory->Write(ptr + 0xAC, alpha);
				g_Memory->Write(ptr + 0x2CE, ghost);
				g_Memory->Write(ptr + 0x708, colMask);
				g_Memory->Write(ptr + 0x94B, bit94b);
				g_Memory->Write(ptr + 0x980, bit980);
				g_Memory->Write(ptr + 0x987, bit987);
				g_Memory->Write(ptr + 0x1A47, bit1a47);
			}
		}

		virtual void OnTick() override
		{
			if (!Pointers.Initialized || !g_Memory)
			{
				m_LastVehiclePointer = 0;
				m_Originals.valid = false;
				return;
			}

			const auto ptr = Self::GetVehiclePtr();

			if (ptr != m_LastVehiclePointer)
			{
				if (m_LastVehiclePointer)
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
			if (!Pointers.Initialized || !g_Memory)
			{
				m_LastVehiclePointer = 0;
				m_Originals.valid = false;
				return;
			}

			if (m_LastVehiclePointer)
			{
				Restore(m_LastVehiclePointer);
				m_LastVehiclePointer = 0;
				m_Originals.valid = false;
			}
			else if (const auto ptr = Self::GetVehiclePtr())
			{
				Restore(ptr);
			}
		}
	};

	static GhostMode _GhostMode{"ghostmode", "Ghost Mode", "Pass through vehicles and objects with transparency"};
}
