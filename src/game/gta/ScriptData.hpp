#pragma once
#include "core/memory/Pattern.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rage
{
	class scrProgram;
}

namespace YimMenu
{
	class ScriptData
	{
		std::vector<std::vector<std::uint8_t>> m_PagesData{};
		std::vector<std::uintptr_t> m_RemotePageAddrs{};
		std::uint32_t m_Size{0};
		std::string m_Name{};
		bool m_Valid{false};

	public:
		ScriptData(std::uintptr_t remoteProgramAddr, const rage::scrProgram& program);
		~ScriptData() = default;

		bool IsValid() const { return m_Valid; }
		std::uint32_t GetSize() const { return m_Size; }
		const std::string& GetName() const { return m_Name; }

		std::uint8_t* GetCodeLocation(std::uint32_t idx);
		std::uintptr_t GetRemoteCodeAddress(std::uint32_t idx) const;

		static std::optional<std::uint32_t> GetCodeLocationByPattern(ScriptData* data, const SimplePattern& pattern);
	};
}
