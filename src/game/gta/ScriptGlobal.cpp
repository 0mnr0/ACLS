#include "ScriptGlobal.hpp"
#include "game/pointers/Pointers.hpp"

namespace YimMenu
{
	std::uintptr_t ScriptGlobal::GetAddress() const
	{
		if (!Pointers.Initialized || !Pointers.ScriptGlobals || !g_Memory)
			return 0;

		const std::size_t pageIndex = (m_Index >> 0x12) & 0x3F;
		auto pageBase = g_Memory->Read<std::uintptr_t>(Pointers.ScriptGlobals + pageIndex * sizeof(std::uintptr_t));
		if (!pageBase.has_value() || !IMemory::IsValidUserAddress(*pageBase))
			return 0;

		return *pageBase + (m_Index & 0x3FFFF) * sizeof(std::int64_t);
	}

	bool ScriptGlobal::CanAccess() const
	{
		return GetAddress() != 0;
	}
}
