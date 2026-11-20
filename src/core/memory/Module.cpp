#include "Module.hpp"

namespace YimMenu
{
	Module::Module(RemoteModuleInfo info) :
	    m_Path(std::move(info.fullPath)),
	    m_Name(std::move(info.name)),
	    m_Base(info.base),
	    m_Size(info.size),
	    m_CodeSize(info.size)
	{
		if (g_Memory && m_Base)
		{
			std::array<std::uint8_t, 0x1000> header{};
			if (g_Memory->ReadBuffer(m_Base, header.data(), header.size(), false))
			{
				const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(header.data());
				if (dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew >= 0 && static_cast<std::size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) <= header.size())
				{
					const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(header.data() + dos->e_lfanew);
					if (nt->Signature == IMAGE_NT_SIGNATURE)
					{
						m_TimeDateStamp = nt->FileHeader.TimeDateStamp;
						if (nt->OptionalHeader.SizeOfCode > 0)
							m_CodeSize = nt->OptionalHeader.SizeOfCode;
					}
				}
			}
		}
	}

	bool Module::CacheImage()
	{
		if (!m_CachedImage.empty())
			return true;

		if (!Valid() || !g_Memory)
			return false;

		const std::size_t scanSize = m_Size;
		m_CachedImage.resize(scanSize, 0);

		constexpr std::size_t chunkSize = 0x100000; // 1 MB chunks
		bool anyRead = false;

		for (std::size_t offset = 0; offset < scanSize; offset += chunkSize)
		{
			const std::size_t toRead = std::min(chunkSize, scanSize - offset);
			if (g_Memory->ReadBuffer(m_Base + offset, m_CachedImage.data() + offset, toRead, false))
			{
				anyRead = true;
			}
			else if (auto scatter = g_Memory->CreateScatterHandle(false))
			{
				for (std::size_t page = 0; page < toRead; page += 0x1000)
				{
					const std::size_t pageBytes = std::min<std::size_t>(0x1000, toRead - page);
					scatter->PrepareRead(m_Base + offset + page, m_CachedImage.data() + offset + page, pageBytes);
				}
				if (scatter->Execute())
					anyRead = true;
			}
		}

		if (!anyRead)
		{
			m_CachedImage.clear();
			return false;
		}

		return true;
	}

	void Module::ClearCache()
	{
		m_CachedImage.clear();
		m_CachedImage.shrink_to_fit();
	}
}
