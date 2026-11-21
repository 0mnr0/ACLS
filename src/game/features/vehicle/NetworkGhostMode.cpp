#include "core/commands/BoolCommand.hpp"
#include "core/commands/IntCommand.hpp"
#include "core/commands/LoopedCommand.hpp"
#include "core/dma/IMemory.hpp"
#include "game/backend/Self.hpp"
#include "game/pointers/Pointers.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>

// ---------------------------------------------------------------------------
// Network Ghost Mode (GTA5_Enhanced).
//
// Found by diffing game memory (via DMA) with the native Network Ghost Mode OFF vs ON:
// the only stable difference is the byte at netObject+0x1C5 of the local ped and of the
// current vehicle (bits 0x44). Setting those bits disables collisions with other players
// and is synced to them. Writing vehicle physics flags was tested and crashes the game,
// so it is intentionally NOT done here.
//
// Optional "Modify Alpha" writes the entity alpha (+0xAC). The game rewrites this value
// every frame, so it flickers; it is off by default.
//
// Safety rules:
//  - only write to entities that are provably alive (netObject+0x50 points back to them);
//  - only write after the same entity/netObject pair has been stable for a while;
//  - only PLAYER (11) and AUTOMOBILE (0) netObjects (the only layouts the offset is verified for);
//  - only our own bits are set/cleared, and only when the byte actually differs.
// ---------------------------------------------------------------------------

namespace YimMenu::Features
{
	/*
	static IntCommand _NetworkGhostAlpha{"networkghostalpha", "Network Ghost Alpha",
	    "Alpha transparency in network ghost mode (0 = invisible, 255 = opaque)", 0, 255, 128};

	static BoolCommand _NetworkGhostModifyAlpha{"networkghostmodifyalpha", "Modify Alpha",
	    "Also write entity alpha (the game fights this every frame, which can cause flickering)", false};
	*/

	static constexpr std::ptrdiff_t k_NetObjectOffset  = 0x00D0; // CDynamicEntity::m_NetObject
	static constexpr std::ptrdiff_t k_NetBackPtrOffset = 0x0050; // netObject::m_GameObject (points back to the entity)
	static constexpr std::ptrdiff_t k_NetTypeOffset    = 0x0008; // netObject::m_ObjectType
	static constexpr std::ptrdiff_t k_NetGhostFlagOff  = 0x01C5; // netObject ghost/collision flag byte
	// static constexpr std::ptrdiff_t k_AlphaOffset      = 0x00AC; // entity alpha
	static constexpr std::uint8_t   k_NetGhostBits     = 0x44;
	static constexpr std::uint16_t  k_TypeAutomobile   = 0;
	static constexpr std::uint16_t  k_TypePlayer       = 11;

	class NetworkGhostMode : public LoopedCommand
	{
		using LoopedCommand::LoopedCommand;
		using Clock = std::chrono::steady_clock;

		static constexpr std::chrono::milliseconds k_StableDelay{2000};

		struct Tracked
		{
			std::uintptr_t    entity{0};     // entity we applied the flag to
			std::uintptr_t    net{0};        // its netObject
			std::uintptr_t    seenKey{0};    // stability tracking
			Clock::time_point seenSince{};
			// std::uint8_t      origAlpha{0xFF};
			// bool              alphaWritten{false};
		};

		Tracked m_Ped;
		Tracked m_Veh;

		// ----------------------------------------------------------------
		// Helpers
		// ----------------------------------------------------------------
		static std::uintptr_t GetNetObject(std::uintptr_t entity)
		{
			if (!g_Memory || !IMemory::IsValidUserAddress(entity))
				return 0;
			const auto n = g_Memory->Read<std::uintptr_t>(entity + k_NetObjectOffset).value_or(0);
			return IMemory::IsValidUserAddress(n) ? n : 0;
		}

		// Live = has a netObject whose back-pointer is this very entity.
		static bool IsLiveEntity(std::uintptr_t entity)
		{
			const auto net = GetNetObject(entity);
			if (!net)
				return false;
			return g_Memory->Read<std::uintptr_t>(net + k_NetBackPtrOffset).value_or(0) == entity;
		}

		static bool IsSupportedNetObject(std::uintptr_t net)
		{
			const auto type = g_Memory->Read<std::uint16_t>(net + k_NetTypeOffset).value_or(0xFFFF);
			return type == k_TypeAutomobile || type == k_TypePlayer;
		}

		// Reads the byte, applies set/clear masks and writes back only if it changed.
		static void ModifyByte(std::uintptr_t addr, std::uint8_t setMask, std::uint8_t clearMask)
		{
			const auto cur = g_Memory->Read<std::uint8_t>(addr);
			if (!cur)
				return;
			const auto nv = static_cast<std::uint8_t>((*cur | setMask) & ~clearMask);
			if (nv != *cur)
				g_Memory->Write<std::uint8_t>(addr, nv);
		}

		static bool IsStable(std::uintptr_t entity, Tracked& t)
		{
			std::uintptr_t key = 0;
			if (entity)
				key = entity ^ (GetNetObject(entity) << 1);

			const auto now = Clock::now();
			if (!key || key != t.seenKey)
			{
				t.seenKey = key;
				t.seenSince = now;
				return false;
			}
			return now - t.seenSince >= k_StableDelay;
		}

		// ----------------------------------------------------------------
		// Apply / restore for one tracked entity
		// ----------------------------------------------------------------
		void Restore(Tracked& t)
		{
			if (t.entity && g_Memory && IsLiveEntity(t.entity) && GetNetObject(t.entity) == t.net)
			{
				ModifyByte(t.net + k_NetGhostFlagOff, 0, k_NetGhostBits);
				// if (t.alphaWritten)
				// 	g_Memory->Write<std::uint8_t>(t.entity + k_AlphaOffset, t.origAlpha);
			}
			t.entity = 0;
			t.net = 0;
			// t.alphaWritten = false;
		}

		void Apply(std::uintptr_t entity, Tracked& t)
		{
			const auto net = GetNetObject(entity);
			if (!net || !IsSupportedNetObject(net))
				return;

			if (entity != t.entity || net != t.net)
			{
				Restore(t);
				t.entity = entity;
				t.net = net;
			}

			ModifyByte(net + k_NetGhostFlagOff, k_NetGhostBits, 0);

			/*
			if (_NetworkGhostModifyAlpha.GetState())
			{
				if (!t.alphaWritten)
				{
					const auto cur = g_Memory->Read<std::uint8_t>(entity + k_AlphaOffset);
					if (!cur)
						return;
					t.origAlpha = *cur;
					t.alphaWritten = true;
				}
				g_Memory->Write<std::uint8_t>(entity + k_AlphaOffset,
				    static_cast<std::uint8_t>(std::clamp(_NetworkGhostAlpha.GetState(), 0, 255)));
			}
			else if (t.alphaWritten)
			{
				g_Memory->Write<std::uint8_t>(entity + k_AlphaOffset, t.origAlpha);
				t.alphaWritten = false;
			}
			*/
		}

		void Handle(std::uintptr_t entity, Tracked& t)
		{
			if (entity && IsLiveEntity(entity) && IsStable(entity, t))
			{
				Apply(entity, t);
				return;
			}

			// Entity gone / not stable yet: never write to the old one unless it is provably the same.
			if (!entity || !IsLiveEntity(entity))
				t.seenKey = 0;
			if (t.entity && entity != t.entity)
				Restore(t);
		}

	public:
		virtual void OnTick() override
		{
			if (!Pointers.Initialized || !g_Memory)
			{
				m_Ped = {};
				m_Veh = {};
				return;
			}

			// Do nothing while no session is running.
			if (Pointers.IsSessionStarted && !g_Memory->Read<bool>(Pointers.IsSessionStarted).value_or(false))
			{
				m_Ped.seenKey = 0;
				m_Veh.seenKey = 0;
				return;
			}

			Handle(Self::GetPedPtr(), m_Ped);
			Handle(Self::GetVehiclePtr(), m_Veh);
		}

		virtual void OnDisable() override
		{
			if (Pointers.Initialized && g_Memory)
			{
				Restore(m_Ped);
				Restore(m_Veh);
			}
			m_Ped = {};
			m_Veh = {};
		}
	};

	static NetworkGhostMode _NetworkGhostMode{"networkghostmode", "Network Ghost Mode",
	    "Disables collisions with other players (synced): sets ghost bits in the local ped/vehicle netObject"};
}
