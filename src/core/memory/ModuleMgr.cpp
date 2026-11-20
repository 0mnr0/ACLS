#include "ModuleMgr.hpp"

#include "core/dma/IMemory.hpp"
#include "core/util/Joaat.hpp"

namespace YimMenu
{
	Module* ModuleMgr::Get(const std::string_view name)
	{
		return Get(Joaat(name));
	}

	Module* ModuleMgr::Get(joaat_t hash)
	{
		if (const auto& it = m_CachedModules.find(hash); it != m_CachedModules.end())
		{
			return it->second.get();
		}
		return nullptr;
	}

	bool ModuleMgr::LoadModules()
	{
		m_CachedModules.clear();

		if (!g_Memory || !g_Memory->IsProcessAttached())
			return false;

		auto modules = g_Memory->GetModules();
		if (modules.empty())
			return false;

		for (auto& info : modules)
		{
			const auto hash = Joaat(info.name);
			auto mod = std::make_unique<Module>(std::move(info));
			m_CachedModules.insert({hash, std::move(mod)});
		}

		return !m_CachedModules.empty();
	}

	std::unordered_multimap<std::uint32_t, std::unique_ptr<Module>>& ModuleMgr::GetModules()
	{
		return m_CachedModules;
	}
}
