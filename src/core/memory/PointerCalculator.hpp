#pragma once
#include "core/dma/IMemory.hpp"
#include <cstdint>
#include <type_traits>

namespace YimMenu
{
	class PointerCalculator final
	{
	private:
		std::uintptr_t m_InternalPtr;

	public:
		inline PointerCalculator(std::uintptr_t ptr = 0);
		~PointerCalculator() = default;

		template<typename T>
		std::enable_if_t<std::is_same_v<T, std::uintptr_t>, T> As() const;

		template<typename T>
		PointerCalculator Add(T offset) const;

		template<typename T>
		PointerCalculator Sub(T offset) const;

		PointerCalculator Rip() const;
		PointerCalculator Deref() const;

		explicit operator bool() const;
		friend bool operator==(const PointerCalculator& a, const PointerCalculator& b);
		friend bool operator!=(const PointerCalculator& a, const PointerCalculator& b);
	};

	inline PointerCalculator::PointerCalculator(std::uintptr_t ptr) :
	    m_InternalPtr(ptr)
	{
	}

	template<typename T>
	inline std::enable_if_t<std::is_same_v<T, std::uintptr_t>, T> PointerCalculator::As() const
	{
		return m_InternalPtr;
	}

	template<typename T>
	inline PointerCalculator PointerCalculator::Add(T offset) const
	{
		return PointerCalculator(m_InternalPtr + static_cast<std::uintptr_t>(offset));
	}

	template<typename T>
	inline PointerCalculator PointerCalculator::Sub(T offset) const
	{
		return PointerCalculator(m_InternalPtr - static_cast<std::uintptr_t>(offset));
	}

	inline PointerCalculator PointerCalculator::Rip() const
	{
		if (!m_InternalPtr || !g_Memory)
			return PointerCalculator(0);

		auto displacement = g_Memory->Read<std::int32_t>(m_InternalPtr);
		if (!displacement.has_value())
			return PointerCalculator(0);

		return PointerCalculator(static_cast<std::uintptr_t>(static_cast<std::int64_t>(m_InternalPtr) + *displacement + 4));
	}

	inline PointerCalculator PointerCalculator::Deref() const
	{
		if (!m_InternalPtr || !g_Memory)
			return PointerCalculator(0);

		auto target = g_Memory->Read<std::uintptr_t>(m_InternalPtr);
		if (!target.has_value())
			return PointerCalculator(0);

		return PointerCalculator(*target);
	}

	inline PointerCalculator::operator bool() const
	{
		return m_InternalPtr != 0;
	}

	inline bool operator==(const PointerCalculator& a, const PointerCalculator& b)
	{
		return a.m_InternalPtr == b.m_InternalPtr;
	}

	inline bool operator!=(const PointerCalculator& a, const PointerCalculator& b)
	{
		return a.m_InternalPtr != b.m_InternalPtr;
	}
}
