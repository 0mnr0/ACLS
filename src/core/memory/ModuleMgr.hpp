#pragma once
#include "Module.hpp"
#include "common.hpp"

namespace YimMenu
{
	using joaat_t = std::uint32_t;

	class ModuleMgr
	{
	public:
		ModuleMgr() = default;
		virtual ~ModuleMgr() = default;
		ModuleMgr(const ModuleMgr&) = delete;
		ModuleMgr(ModuleMgr&&) noexcept = delete;
		ModuleMgr& operator=(const ModuleMgr&) = delete;
		ModuleMgr& operator=(ModuleMgr&&) noexcept = delete;

		Module* Get(std::string_view name);
		Module* Get(joaat_t hash);

		bool LoadModules();
		std::unordered_multimap<std::uint32_t, std::unique_ptr<Module>>& GetModules();

	private:
		std::unordered_multimap<std::uint32_t, std::unique_ptr<Module>> m_CachedModules;
	};

	inline YimMenu::ModuleMgr ModuleMgr;
}
