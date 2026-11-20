#pragma once
#include "core/dma/IMemory.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace YimMenu
{
	class ScriptGlobal
	{
	private:
		std::size_t m_Index;

	public:
		constexpr ScriptGlobal(std::size_t idx) :
		    m_Index(idx)
		{
		}

		constexpr ScriptGlobal At(std::ptrdiff_t offset) const
		{
			return m_Index + offset;
		}

		constexpr ScriptGlobal At(std::ptrdiff_t offset, std::size_t size) const
		{
			return m_Index + 1 + offset * size;
		}

		constexpr bool operator==(const ScriptGlobal& other) const
		{
			return m_Index == other.m_Index;
		}

		bool CanAccess() const;
		std::uintptr_t GetAddress() const;

		template<typename T>
		std::optional<T> Read() const
		{
			static_assert(std::is_trivially_copyable_v<T>);
			const auto addr = GetAddress();
			if (!addr || !g_Memory)
				return std::nullopt;
			return g_Memory->Read<T>(addr);
		}

		template<typename T>
		bool Write(const T& value) const
		{
			static_assert(std::is_trivially_copyable_v<T>);
			const auto addr = GetAddress();
			if (!addr || !g_Memory)
				return false;
			return g_Memory->Write<T>(addr, value);
		}
	};
}
