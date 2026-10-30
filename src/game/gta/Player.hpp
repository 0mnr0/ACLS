#pragma once
#include "Ped.hpp"

class CNetGamePlayer;

namespace YimMenu
{
	class Player
	{
		CNetGamePlayer* m_Handle{nullptr};

	public:
		Player() = default;
		Player(uint8_t id) : m_Handle(nullptr) {}
		constexpr Player(CNetGamePlayer* netGamePlayer) : m_Handle(netGamePlayer) {}

		bool IsValid() const { return m_Handle != nullptr; }
		bool IsLocal() const { return false; }
		int GetId() const { return 0; }
		const char* GetName() { return ""; }
		CNetGamePlayer* GetHandle() { return m_Handle; }
		Ped GetPed() { return Ped(nullptr); }

		bool operator==(Player other) { return m_Handle == other.m_Handle; }
		inline operator bool() { return IsValid(); }
	};
}
