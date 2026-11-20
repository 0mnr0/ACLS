#include "ScriptPointer.hpp"

namespace YimMenu
{
	static std::uint32_t ReadThreeByte(const std::uint8_t* arr)
	{
		return arr[0] + (arr[1] << 8) + (arr[2] << 16);
	}

	ScriptPointer::ScriptPointer(const std::string& name, const SimplePattern& pattern, std::int32_t offset, bool rip, std::uint32_t address) :
	    m_Name(name),
	    m_Pattern(pattern),
	    m_Offset(offset),
	    m_Rip(rip),
	    m_Address(address)
	{
	}

	ScriptPointer ScriptPointer::Add(std::uint32_t offset)
	{
		return ScriptPointer(m_Name, m_Pattern, m_Offset + static_cast<std::int32_t>(offset), m_Rip, m_Address);
	}

	ScriptPointer ScriptPointer::Sub(std::uint32_t offset)
	{
		return ScriptPointer(m_Name, m_Pattern, m_Offset - static_cast<std::int32_t>(offset), m_Rip, m_Address);
	}

	ScriptPointer ScriptPointer::Rip()
	{
		return ScriptPointer(m_Name, m_Pattern, m_Offset, true, m_Address);
	}

	ScriptPointer ScriptPointer::Scan(ScriptData* data)
	{
		if (auto location = ScriptData::GetCodeLocationByPattern(data, m_Pattern))
		{
			m_Address = static_cast<std::uint32_t>(static_cast<std::int64_t>(location.value()) + m_Offset);
			if (m_Rip)
			{
				if (const auto* ptr = data->GetCodeLocation(m_Address))
					m_Address = ReadThreeByte(ptr);
				else
					m_Address = 0;
			}

			if (m_Address)
			{
				LOG(INFO) << "Found pattern for " << m_Name << " at " << HEX(m_Address) << " in script " << data->GetName();
				return ScriptPointer(m_Name, m_Pattern, m_Offset, m_Rip, m_Address);
			}
		}

		LOG(WARNING) << "Failed to find pattern for " << m_Name;
		return ScriptPointer(m_Name, m_Pattern, m_Offset, m_Rip, 0);
	}
}
