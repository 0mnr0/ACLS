#pragma once
#include "PointerCalculator.hpp"
#include "common.hpp"
#include "core/dma/IMemory.hpp"

namespace YimMenu
{
	class Module
	{
	public:
		explicit Module(RemoteModuleInfo info);
		~Module() = default;

		std::string_view Name() const { return m_Name; }
		std::uintptr_t Base() const { return m_Base; }
		std::uintptr_t Size() const { return m_Size; }
		std::uintptr_t End() const { return m_Base + m_Size; }
		std::uintptr_t CodeSize() const { return m_CodeSize; }
		std::uint32_t TimeDateStamp() const { return m_TimeDateStamp; }

		bool Valid() const { return m_Base != 0 && m_Size != 0; }

		bool CacheImage();
		void ClearCache();
		const std::vector<std::uint8_t>& GetCachedImage() const { return m_CachedImage; }

	private:
		std::string m_Path;
		std::string m_Name;
		std::uintptr_t m_Base{0};
		std::uintptr_t m_Size{0};
		std::uintptr_t m_CodeSize{0};
		std::uint32_t m_TimeDateStamp{0};
		std::vector<std::uint8_t> m_CachedImage{};
	};
}
