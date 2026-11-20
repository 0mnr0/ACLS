#include "core/commands/BoolCommand.hpp"
#include "core/commands/IntCommand.hpp"
#include "core/commands/BoolCommand.hpp"
#include "core/commands/LoopedCommand.hpp"
#include "core/dma/IMemory.hpp"
#include "game/backend/Self.hpp"
#include "game/pointers/Pointers.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
// ---------------------------------------------------------------------------
// Findings (GTA5_Enhanced, DMA memory diff vs. the native Network Ghost Mode of the
// "enhanced" branch, OFF vs ON): the only stable difference is in the netObject of
// the local ped and current vehicle, byte netObject+0x1C5 (vehicle also +0x1C6):
//     ped: 0x01 -> 0x45    vehicle: 0x00 -> 0xC4 (+0x1C6: 0x00 -> 0x02)
// Bits 0x04 and 0x40 are common to both; the game does not overwrite them.
// Entity alpha (+0xAC) is rewritten by the game every frame -> written every tick.
// Whether REMOTE players see the ghost state must be verified in a public session.
// ---------------------------------------------------------------------------

namespace YimMenu::Features
{
	static IntCommand _NetworkGhostAlpha{"networkghostalpha", "Network Ghost Alpha",
	    "Alpha transparency in network ghost mode (0 = invisible, 255 = opaque)", 0, 255, 128};

	static BoolCommand _NetworkGhostModifyAlpha{"networkghostmodifyalpha", "Modify Alpha",
	    "Also write entity alpha (the game fights this every frame, which can cause flickering)", false};

	static constexpr std::ptrdiff_t k_NetObjectOffset = 0x00D0; // CDynamicEntity::m_NetObject
	static constexpr std::ptrdiff_t k_NetGhostFlagOff = 0x01C5; // netObject ghost/collision flag byte
	static constexpr std::uint8_t   k_NetGhostBits    = 0x44;

	// -----------------------------------------------------------------------
	// Ghost / collision state applied to the local ped/vehicle via raw DMA
	// writes (mirrors what GhostMode.cpp does for the physics engine flags).
	// -----------------------------------------------------------------------
	struct OriginalVehicleValues
	{
		std::uint8_t  alpha{0xFF};
		std::uint8_t  ghost{0};
		std::uint32_t col_mask{0};
		std::uint8_t  bit_94b{0};
		std::uint8_t  bit_980{0x04};
		std::uint8_t  bit_987{0};
		std::uint8_t  bit_1a47{0};
		bool          valid{false};
	};

	class NetworkGhostMode : public LoopedCommand
	{
		using LoopedCommand::LoopedCommand;

		// --- Visual alpha (ped + vehicle) ---
		std::uintptr_t m_LastVehiclePointer{0};
		std::uintptr_t m_LastPedPointer{0};
		std::uint8_t   m_OrigVehicleAlpha{0xFF};
		std::uint8_t   m_OrigPedAlpha{0xFF};
		bool           m_OrigVehicleAlphaValid{false};
		bool           m_OrigPedAlphaValid{false};

		// Stability gate: a freshly created ped/vehicle (session change, respawn, new car) is not
		// written to until the same entity + netObject pair has been seen continuously for a while.
		using Clock = std::chrono::steady_clock;
		std::uintptr_t    m_SeenPed{0};
		std::uintptr_t    m_SeenVeh{0};
		Clock::time_point m_PedSince{};
		Clock::time_point m_VehSince{};
		static constexpr std::chrono::milliseconds k_StableDelay{2000};

		bool IsStable(std::uintptr_t entity, std::uintptr_t& seen, Clock::time_point& since)
		{
			const auto now = Clock::now();
			std::uintptr_t key = 0;
			if (entity)
			{
				const auto net = g_Memory->Read<std::uintptr_t>(entity + k_NetObjectOffset).value_or(0);
				key = entity ^ (net << 1);
			}
			if (!key || key != seen)
			{
				seen = key;
				since = now;
				return false;
			}
			return now - since >= k_StableDelay;
		}

		bool           m_AlphaWritten{false}; // we have written alpha to ped/vehicle and must restore it

		// --- Physics ghost flags (vehicle) ---
		OriginalVehicleValues m_VehOriginals{};

		// --- netObject ghost flag state (ped + vehicle) ---
		std::uintptr_t m_PedNetEntity{0};
		std::uintptr_t m_VehNetEntity{0};
		std::uintptr_t m_PedNetObj{0};
		std::uintptr_t m_VehNetObj{0};
		std::uint8_t   m_PedNetOrig{0};
		std::uint8_t   m_VehNetOrig{0};

		// ----------------------------------------------------------------
		// Alpha helpers
		// ----------------------------------------------------------------
		void ApplyAlpha(std::uintptr_t ptr, std::uint8_t& origAlpha, bool& origValid)
		{
			if (!g_Memory || !IMemory::IsValidUserAddress(ptr))
				return;

			if (!origValid)
			{
				const auto cur = g_Memory->Read<std::uint8_t>(ptr + 0xAC);
				if (cur.has_value())
				{
					origAlpha = *cur;
					origValid = true;
				}
			}

			const auto alpha = static_cast<std::uint8_t>(std::clamp(_NetworkGhostAlpha.GetState(), 0, 255));
			g_Memory->Write<std::uint8_t>(ptr + 0xAC, alpha);
			m_AlphaWritten = true;
		}

		void RestoreAlpha(std::uintptr_t ptr, std::uint8_t origAlpha, bool& origValid)
		{
			if (!g_Memory || !IMemory::IsValidUserAddress(ptr))
			{
				origValid = false;
				return;
			}

			if (origValid)
				g_Memory->Write<std::uint8_t>(ptr + 0xAC, origAlpha);
			origValid = false;
		}

		// ----------------------------------------------------------------
		// Physics ghost-flag helpers (vehicle only, same as GhostMode.cpp)
		// ----------------------------------------------------------------
		void SaveVehicleOriginals(std::uintptr_t ptr)
		{
			if (!g_Memory || !IMemory::IsValidUserAddress(ptr))
				return;

			OriginalVehicleValues vals{};
			if (auto scatter = g_Memory->CreateScatterHandle())
			{
				scatter->PrepareRead(ptr + 0xAC,   vals.alpha);
				scatter->PrepareRead(ptr + 0x2CE,  vals.ghost);
				scatter->PrepareRead(ptr + 0x708,  vals.col_mask);
				scatter->PrepareRead(ptr + 0x94B,  vals.bit_94b);
				scatter->PrepareRead(ptr + 0x980,  vals.bit_980);
				scatter->PrepareRead(ptr + 0x987,  vals.bit_987);
				scatter->PrepareRead(ptr + 0x1A47, vals.bit_1a47);
				if (scatter->Execute())
				{
					vals.valid   = true;
					m_VehOriginals = vals;
					return;
				}
			}

			// Scatter not available — individual reads
			const auto alpha   = g_Memory->Read<std::uint8_t>(ptr + 0xAC);
			const auto ghost   = g_Memory->Read<std::uint8_t>(ptr + 0x2CE);
			const auto colMask = g_Memory->Read<std::uint32_t>(ptr + 0x708);
			const auto bit94b  = g_Memory->Read<std::uint8_t>(ptr + 0x94B);
			const auto bit980  = g_Memory->Read<std::uint8_t>(ptr + 0x980);
			const auto bit987  = g_Memory->Read<std::uint8_t>(ptr + 0x987);
			const auto bit1a47 = g_Memory->Read<std::uint8_t>(ptr + 0x1A47);

			if (alpha && ghost && colMask && bit94b && bit980 && bit987 && bit1a47)
			{
				m_VehOriginals.alpha   = *alpha;
				m_VehOriginals.ghost   = *ghost;
				m_VehOriginals.col_mask = *colMask;
				m_VehOriginals.bit_94b  = *bit94b;
				m_VehOriginals.bit_980  = *bit980;
				m_VehOriginals.bit_987  = *bit987;
				m_VehOriginals.bit_1a47 = *bit1a47;
				m_VehOriginals.valid    = true;
			}
		}

		void ApplyVehicleGhostFlags(std::uintptr_t ptr)
		{
			if (!g_Memory || !IMemory::IsValidUserAddress(ptr))
				return;

			std::uint8_t bit94b = 0, bit980 = 0, bit987 = 0, bit1a47 = 0;
			bool readOk = false;

			if (auto scatter = g_Memory->CreateScatterHandle())
			{
				scatter->PrepareRead(ptr + 0x94B,  bit94b);
				scatter->PrepareRead(ptr + 0x980,  bit980);
				scatter->PrepareRead(ptr + 0x987,  bit987);
				scatter->PrepareRead(ptr + 0x1A47, bit1a47);
				readOk = scatter->Execute();
			}

			if (!readOk)
			{
				const auto b1 = g_Memory->Read<std::uint8_t>(ptr + 0x94B);
				const auto b2 = g_Memory->Read<std::uint8_t>(ptr + 0x980);
				const auto b3 = g_Memory->Read<std::uint8_t>(ptr + 0x987);
				const auto b4 = g_Memory->Read<std::uint8_t>(ptr + 0x1A47);
				if (!b1 || !b2 || !b3 || !b4)
					return;
				bit94b  = *b1;
				bit980  = *b2;
				bit987  = *b3;
				bit1a47 = *b4;
			}

			const auto alpha = static_cast<std::uint8_t>(std::clamp(_NetworkGhostAlpha.GetState(), 0, 255));
			constexpr std::uint8_t  ghost   = 1;
			constexpr std::uint32_t colMask = 0xFFFFFFFF;
			bit94b  |=  0x80;
			bit980  &= ~0x04;
			bit987  |=  0x02;
			bit1a47 |=  0x80;

			const bool modAlpha = _NetworkGhostModifyAlpha.GetState();
			if (modAlpha)
				m_AlphaWritten = true;

			if (auto scatter = g_Memory->CreateScatterHandle())
			{
				if (modAlpha)
					scatter->PrepareWrite(ptr + 0xAC,   alpha);
				scatter->PrepareWrite(ptr + 0x2CE,  ghost);
				scatter->PrepareWrite(ptr + 0x708,  colMask);
				scatter->PrepareWrite(ptr + 0x94B,  bit94b);
				scatter->PrepareWrite(ptr + 0x980,  bit980);
				scatter->PrepareWrite(ptr + 0x987,  bit987);
				scatter->PrepareWrite(ptr + 0x1A47, bit1a47);
				scatter->Execute();
			}
			else
			{
				if (modAlpha)
					g_Memory->Write(ptr + 0xAC,   alpha);
				g_Memory->Write(ptr + 0x2CE,  ghost);
				g_Memory->Write(ptr + 0x708,  colMask);
				g_Memory->Write(ptr + 0x94B,  bit94b);
				g_Memory->Write(ptr + 0x980,  bit980);
				g_Memory->Write(ptr + 0x987,  bit987);
				g_Memory->Write(ptr + 0x1A47, bit1a47);
			}
		}

		void RestoreVehicleGhostFlags(std::uintptr_t ptr)
		{
			if (!g_Memory || !IMemory::IsValidUserAddress(ptr))
				return;

			if (m_VehOriginals.valid)
			{
				if (auto scatter = g_Memory->CreateScatterHandle())
				{
					if (m_AlphaWritten)
						scatter->PrepareWrite(ptr + 0xAC,   m_VehOriginals.alpha);
					scatter->PrepareWrite(ptr + 0x2CE,  m_VehOriginals.ghost);
					scatter->PrepareWrite(ptr + 0x708,  m_VehOriginals.col_mask);
					scatter->PrepareWrite(ptr + 0x94B,  m_VehOriginals.bit_94b);
					scatter->PrepareWrite(ptr + 0x980,  m_VehOriginals.bit_980);
					scatter->PrepareWrite(ptr + 0x987,  m_VehOriginals.bit_987);
					scatter->PrepareWrite(ptr + 0x1A47, m_VehOriginals.bit_1a47);
					scatter->Execute();
				}
				else
				{
					if (m_AlphaWritten)
						g_Memory->Write(ptr + 0xAC,   m_VehOriginals.alpha);
					g_Memory->Write(ptr + 0x2CE,  m_VehOriginals.ghost);
					g_Memory->Write(ptr + 0x708,  m_VehOriginals.col_mask);
					g_Memory->Write(ptr + 0x94B,  m_VehOriginals.bit_94b);
					g_Memory->Write(ptr + 0x980,  m_VehOriginals.bit_980);
					g_Memory->Write(ptr + 0x987,  m_VehOriginals.bit_987);
					g_Memory->Write(ptr + 0x1A47, m_VehOriginals.bit_1a47);
				}
			}
			else
			{
				// Safe defaults when originals were not captured
				const auto bit94b  = g_Memory->Read<std::uint8_t>(ptr + 0x94B).value_or(0);
				const auto bit980  = g_Memory->Read<std::uint8_t>(ptr + 0x980).value_or(0);
				const auto bit987  = g_Memory->Read<std::uint8_t>(ptr + 0x987).value_or(0);
				const auto bit1a47 = g_Memory->Read<std::uint8_t>(ptr + 0x1A47).value_or(0);

				if (m_AlphaWritten)
					g_Memory->Write<std::uint8_t> (ptr + 0xAC,   0xFF);
				g_Memory->Write<std::uint8_t> (ptr + 0x2CE,  0);
				g_Memory->Write<std::uint32_t>(ptr + 0x708,  0);
				g_Memory->Write<std::uint8_t> (ptr + 0x94B,  bit94b  & ~0x80);
				g_Memory->Write<std::uint8_t> (ptr + 0x980,  bit980  |  0x04);
				g_Memory->Write<std::uint8_t> (ptr + 0x987,  bit987  & ~0x02);
				g_Memory->Write<std::uint8_t> (ptr + 0x1A47, bit1a47 & ~0x80);
			}

			m_VehOriginals.valid = false;
		}

		// ----------------------------------------------------------------
		// netObject ghost flag (netObject+0x1C5, bits 0x44)
		// ----------------------------------------------------------------
		std::uintptr_t GetNetObject(std::uintptr_t entity)
		{
			if (!g_Memory || !IMemory::IsValidUserAddress(entity))
				return 0;
			const auto n = g_Memory->Read<std::uintptr_t>(entity + k_NetObjectOffset).value_or(0);
			return IMemory::IsValidUserAddress(n) ? n : 0;
		}

		// An entity is "live" if it still has a netObject whose back-pointer (netObject+0x50)
		// points to this very entity. Verified via DMA: for both ped and vehicle
		// netObject+0x50 == entity. Used before EVERY write so we never write into freed /
		// reused memory (session change, respawn, vehicle deleted) - that crashes the game.
		static bool IsLiveEntity(std::uintptr_t entity)
		{
			if (!g_Memory || !IMemory::IsValidUserAddress(entity))
				return false;
			const auto net = g_Memory->Read<std::uintptr_t>(entity + k_NetObjectOffset).value_or(0);
			if (!IMemory::IsValidUserAddress(net))
				return false;
			const auto back = g_Memory->Read<std::uintptr_t>(net + 0x50).value_or(0);
			return back == entity;
		}

		void ApplyNetFlag(std::uintptr_t entity, std::uintptr_t& cachedEntity, std::uintptr_t& cachedNet, std::uint8_t& orig)
		{
			const auto net = GetNetObject(entity);
			if (!net || !IsLiveEntity(entity))
				return;
			if (net != cachedNet || entity != cachedEntity)
			{
				// Old object: restore only if it is provably still the same live object.
				RestoreNetFlag(cachedEntity, cachedNet, orig);
				const auto cur = g_Memory->Read<std::uint8_t>(net + k_NetGhostFlagOff);
				if (!cur)
					return;
				orig = *cur;
				cachedNet = net;
				cachedEntity = entity;
			}
			g_Memory->Write<std::uint8_t>(net + k_NetGhostFlagOff, static_cast<std::uint8_t>(orig | k_NetGhostBits));
		}

		void RestoreNetFlag(std::uintptr_t& cachedEntity, std::uintptr_t& cachedNet, std::uint8_t orig)
		{
			if (cachedNet && cachedEntity && g_Memory && IsLiveEntity(cachedEntity)
			    && GetNetObject(cachedEntity) == cachedNet)
				g_Memory->Write<std::uint8_t>(cachedNet + k_NetGhostFlagOff, orig);
			cachedNet = 0;
			cachedEntity = 0;
		}

		// ----------------------------------------------------------------
		// State reset helpers
		// ----------------------------------------------------------------
		void ResetEntityState()
		{
			m_LastVehiclePointer     = 0;
			m_LastPedPointer         = 0;
			m_OrigVehicleAlphaValid  = false;
			m_OrigPedAlphaValid      = false;
			m_VehOriginals.valid     = false;
			m_PedNetObj              = 0;
			m_VehNetObj              = 0;
			m_PedNetEntity           = 0;
			m_VehNetEntity           = 0;
			m_AlphaWritten           = false;
		}

		// ================================================================
		// LoopedCommand overrides
		// ================================================================
		virtual void OnTick() override
		{
			if (!Pointers.Initialized || !g_Memory)
			{
				ResetEntityState();
				return;
			}

			// Only act on entities that are provably alive right now.
			const auto pedRaw = Self::GetPedPtr();
			const auto vehRaw = Self::GetVehiclePtr();
			// Do nothing while no session is running or while the entities are not stable yet.
			if (Pointers.IsSessionStarted && !g_Memory->Read<bool>(Pointers.IsSessionStarted).value_or(false))
			{
				m_SeenPed = m_SeenVeh = 0;
				return;
			}
			const bool pedOk = IsLiveEntity(pedRaw) && IsStable(pedRaw, m_SeenPed, m_PedSince);
			const bool vehOk = IsLiveEntity(vehRaw) && IsStable(vehRaw, m_SeenVeh, m_VehSince);
			if (!IsLiveEntity(vehRaw))
				m_SeenVeh = 0;
			const auto pedPtr = pedOk ? pedRaw : 0;
			const auto vehPtr = vehOk ? vehRaw : 0;

			// --- Vehicle: physics ghost flags + visual alpha ---
			if (vehPtr != m_LastVehiclePointer)
			{
				// The old vehicle may be gone (session change / deleted): never write to a dead one.
				if (m_LastVehiclePointer && IsLiveEntity(m_LastVehiclePointer))
				{
					RestoreVehicleGhostFlags(m_LastVehiclePointer);
				}
				m_OrigVehicleAlphaValid = false;
				m_LastVehiclePointer = vehPtr;
				m_VehOriginals.valid = false;
				if (vehPtr)
					SaveVehicleOriginals(vehPtr);
			}

			if (vehPtr)
			{
				ApplyVehicleGhostFlags(vehPtr);

				// Restore ped alpha when mounting vehicle
				if (m_LastPedPointer)
				{
					if (IsLiveEntity(m_LastPedPointer))
						RestoreAlpha(m_LastPedPointer, m_OrigPedAlpha, m_OrigPedAlphaValid);
					m_OrigPedAlphaValid = false;
					m_LastPedPointer = 0;
				}
			}
			else
			{
				// --- On foot: visual alpha only ---
				if (pedPtr != m_LastPedPointer)
				{
					if (m_LastPedPointer && IsLiveEntity(m_LastPedPointer))
						RestoreAlpha(m_LastPedPointer, m_OrigPedAlpha, m_OrigPedAlphaValid);
					m_LastPedPointer    = pedPtr;
					m_OrigPedAlphaValid = false;
				}
				if (pedPtr && _NetworkGhostModifyAlpha.GetState())
					ApplyAlpha(pedPtr, m_OrigPedAlpha, m_OrigPedAlphaValid);
			}

			// --- netObject ghost flag (synced state) every tick ---
			if (pedPtr)
				ApplyNetFlag(pedPtr, m_PedNetEntity, m_PedNetObj, m_PedNetOrig);
			if (vehPtr)
				ApplyNetFlag(vehPtr, m_VehNetEntity, m_VehNetObj, m_VehNetOrig);
			else if (m_VehNetObj)
				RestoreNetFlag(m_VehNetEntity, m_VehNetObj, m_VehNetOrig);
		}

		virtual void OnDisable() override
		{
			if (!Pointers.Initialized || !g_Memory)
			{
				ResetEntityState();
				return;
			}

			// Restore vehicle
			if (m_LastVehiclePointer)
			{
				if (IsLiveEntity(m_LastVehiclePointer))
					RestoreVehicleGhostFlags(m_LastVehiclePointer);
				m_LastVehiclePointer = 0;
			}
			else if (const auto vehPtr = Self::GetVehiclePtr(); IsLiveEntity(vehPtr))
			{
				RestoreVehicleGhostFlags(vehPtr);
			}

			// Restore vehicle alpha
			if (m_OrigVehicleAlphaValid)
			{
				const auto vehPtr = Self::GetVehiclePtr();
				if (vehPtr && IsLiveEntity(vehPtr))
					RestoreAlpha(vehPtr, m_OrigVehicleAlpha, m_OrigVehicleAlphaValid);
			}

			// Restore ped alpha
			if (m_LastPedPointer)
			{
				if (IsLiveEntity(m_LastPedPointer))
					RestoreAlpha(m_LastPedPointer, m_OrigPedAlpha, m_OrigPedAlphaValid);
				m_LastPedPointer = 0;
			}
			else if (const auto pedPtr = Self::GetPedPtr(); IsLiveEntity(pedPtr))
			{
				RestoreAlpha(pedPtr, m_OrigPedAlpha, m_OrigPedAlphaValid);
			}

			// Restore netObject ghost flags
			RestoreNetFlag(m_PedNetEntity, m_PedNetObj, m_PedNetOrig);
			RestoreNetFlag(m_VehNetEntity, m_VehNetObj, m_VehNetOrig);

			ResetEntityState();
		}
	};

	static NetworkGhostMode _NetworkGhostMode{"networkghostmode", "Network Ghost Mode",
	    "Ghost mode: vehicle physics flags, alpha and netObject ghost flag (netObject+0x1C5)"};
}
