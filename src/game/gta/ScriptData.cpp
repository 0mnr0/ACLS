#include "ScriptData.hpp"
#include "core/dma/IMemory.hpp"
#include "types/script/scrProgram.hpp"

namespace YimMenu
{
	ScriptData::ScriptData(std::uintptr_t remoteProgramAddr, const rage::scrProgram& program) :
	    m_Size(program.m_CodeSize)
	{
		if (!g_Memory || !program.m_CodeBlocks || !program.m_CodeSize)
			return;

		const auto codeBlocksPtr = reinterpret_cast<std::uintptr_t>(program.m_CodeBlocks);
		if (!IMemory::IsValidUserAddress(codeBlocksPtr))
			return;

		if (const auto namePtr = reinterpret_cast<std::uintptr_t>(program.m_Name); IMemory::IsValidUserAddress(namePtr))
		{
			char nameBuf[64]{};
			if (g_Memory->ReadBuffer(namePtr, nameBuf, sizeof(nameBuf) - 1, false))
				m_Name = nameBuf;
		}
		if (m_Name.empty())
			m_Name = std::format("0x{:X}", program.m_NameHash);

		const std::uint32_t numPages = program.GetNumCodePages();
		m_RemotePageAddrs.resize(numPages, 0);
		if (!g_Memory->ReadBuffer(codeBlocksPtr, m_RemotePageAddrs.data(), numPages * sizeof(std::uintptr_t), false))
			return;

		m_PagesData.resize(numPages);
		for (std::uint32_t i = 0; i < numPages; ++i)
		{
			const std::uint32_t pageSize = program.GetCodePageSize(i);
			if (!IMemory::IsValidUserAddress(m_RemotePageAddrs[i]) || pageSize == 0)
				return;

			m_PagesData[i].resize(pageSize);
			if (!g_Memory->ReadBuffer(m_RemotePageAddrs[i], m_PagesData[i].data(), pageSize, false))
				return;
		}

		m_Valid = true;
	}

	std::uint8_t* ScriptData::GetCodeLocation(std::uint32_t idx)
	{
		const std::uint32_t page = idx >> 14;
		const std::uint32_t offset = idx & 0x3FFF;
		if (page >= m_PagesData.size() || offset >= m_PagesData[page].size())
			return nullptr;
		return &m_PagesData[page][offset];
	}

	std::uintptr_t ScriptData::GetRemoteCodeAddress(std::uint32_t idx) const
	{
		const std::uint32_t page = idx >> 14;
		const std::uint32_t offset = idx & 0x3FFF;
		if (page >= m_RemotePageAddrs.size() || !IMemory::IsValidUserAddress(m_RemotePageAddrs[page]))
			return 0;
		return m_RemotePageAddrs[page] + offset;
	}

	std::optional<std::uint32_t> ScriptData::GetCodeLocationByPattern(ScriptData* data, const SimplePattern& pattern)
	{
		if (!data || !data->IsValid())
			return std::nullopt;

		const std::uint32_t codeSize = data->GetSize();
		if (codeSize <= pattern.m_Bytes.size())
			return std::nullopt;

		for (std::uint32_t i = 0; i < (codeSize - pattern.m_Bytes.size()); ++i)
		{
			bool match = true;
			for (std::uint32_t j = 0; j < pattern.m_Bytes.size(); ++j)
			{
				if (pattern.m_Bytes[j].has_value())
				{
					const auto* bytePtr = data->GetCodeLocation(i + j);
					if (!bytePtr || pattern.m_Bytes[j].value() != *bytePtr)
					{
						match = false;
						break;
					}
				}
			}

			if (match)
				return i;
		}

		return std::nullopt;
	}
}
