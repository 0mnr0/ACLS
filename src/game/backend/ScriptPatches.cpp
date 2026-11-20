#include "ScriptPatches.hpp"
#include "core/dma/IMemory.hpp"
#include "game/pointers/Pointers.hpp"
#include "types/script/scrProgram.hpp"
#include <array>

namespace YimMenu
{
	ScriptPatches::Patch::Patch(joaat_t script, ScriptPointer pointer, std::vector<std::uint8_t> patch) :
	    m_PatchedBytes(std::move(patch)),
	    m_Pointer(std::move(pointer)),
	    m_Hash(script)
	{
	}

	std::optional<std::uint32_t> ScriptPatches::Patch::GetPC()
	{
		if (m_Pc.has_value())
			return m_Pc;

		auto data = ScriptPatches::GetInstance().GetDataImpl(m_Hash);
		if (!data || !data->IsValid())
			return std::nullopt;

		auto ptr = m_Pointer.Scan(data);
		if (auto addr = ptr.As<std::uint32_t>())
		{
			m_Pc = addr;
			return m_Pc;
		}

		return std::nullopt;
	}

	void ScriptPatches::Patch::Apply()
	{
		if (!g_Memory || !Pointers.Initialized)
			return;

		auto data = ScriptPatches::GetInstance().GetDataImpl(m_Hash);
		const auto progAddr = ScriptPatches::GetInstance().GetProgramAddrImpl(m_Hash);
		if (!data || !data->IsValid() || !progAddr)
			return;

		if (m_Applied && m_AppliedProgramAddr == progAddr)
			return;

		// Program reloaded at a new address: invalidate cached PC before scanning again.
		if (m_AppliedProgramAddr != progAddr)
			m_Pc.reset();

		auto pc = GetPC();
		if (!pc.has_value())
		{
			LOG(WARNING) << "ScriptPatch failed to find PC for script: " << data->GetName();
			return;
		}

		const std::size_t len = m_PatchedBytes.size();
		if (*pc + len > data->GetSize())
			return;

		m_OriginalBytes.resize(len);
		for (std::size_t i = 0; i < len; ++i)
		{
			const auto remoteAddr = data->GetRemoteCodeAddress(*pc + static_cast<std::uint32_t>(i));
			if (!IMemory::IsValidUserAddress(remoteAddr))
				return;

			auto original = g_Memory->Read<std::uint8_t>(remoteAddr);
			if (!original.has_value())
				return;

			m_OriginalBytes[i] = *original;
		}

		for (std::size_t i = 0; i < len; ++i)
		{
			const auto remoteAddr = data->GetRemoteCodeAddress(*pc + static_cast<std::uint32_t>(i));
			if (!g_Memory->Write<std::uint8_t>(remoteAddr, m_PatchedBytes[i]))
			{
				LOG(WARNING) << "ScriptPatch failed to write remote bytecode at " << HEX(remoteAddr);
				return;
			}
		}

		m_Applied = true;
		m_AppliedProgramAddr = progAddr;
		LOG(INFO) << "Applied remote script patch in " << data->GetName() << " at PC 0x" << std::hex << *pc << std::dec;
	}

	void ScriptPatches::Patch::Restore()
	{
		if (!m_Applied || !m_Pc.has_value() || m_OriginalBytes.empty())
		{
			ResetRemoteState();
			return;
		}

		if (!g_Memory || !Pointers.Initialized)
		{
			ResetRemoteState();
			return;
		}

		auto data = ScriptPatches::GetInstance().GetDataImpl(m_Hash);
		const auto currentProgAddr = ScriptPatches::GetInstance().GetProgramAddrImpl(m_Hash);
		if (data && data->IsValid() && currentProgAddr == m_AppliedProgramAddr)
		{
			for (std::size_t i = 0; i < m_OriginalBytes.size(); ++i)
			{
				const auto remoteAddr = data->GetRemoteCodeAddress(*m_Pc + static_cast<std::uint32_t>(i));
				if (IMemory::IsValidUserAddress(remoteAddr))
				{
					g_Memory->Write<std::uint8_t>(remoteAddr, m_OriginalBytes[i]);
				}
			}
			LOG(INFO) << "Restored remote script patch in " << data->GetName();
		}

		ResetRemoteState();
	}

	void ScriptPatches::Patch::ResetRemoteState()
	{
		m_Applied = false;
		m_AppliedProgramAddr = 0;
		m_OriginalBytes.clear();
		m_Pc.reset();
	}

	void ScriptPatches::Patch::Enable()
	{
		std::lock_guard lock(ScriptPatches::GetInstance().m_Mutex);
		if (m_Enabled && m_Applied)
			return;

		m_Enabled = true;
		if (Pointers.Initialized)
		{
			ScriptPatches::GetInstance().RefreshProgramsImpl(true);
			Apply();
		}
	}

	void ScriptPatches::Patch::Disable()
	{
		std::lock_guard lock(ScriptPatches::GetInstance().m_Mutex);
		m_Enabled = false;
		if (m_Applied)
		{
			if (Pointers.Initialized)
				ScriptPatches::GetInstance().RefreshProgramsImpl(true);
			Restore();
		}
	}

	void ScriptPatches::Patch::Update()
	{
		if (m_Enabled)
			Apply();
		else if (m_Applied)
			Restore();
	}

	bool ScriptPatches::Patch::InScope(std::uint32_t hash) const
	{
		return m_Hash == hash;
	}

	ScriptPatches::~ScriptPatches()
	{
		std::lock_guard lock(m_Mutex);
		for (auto& patch : m_Patches)
		{
			if (patch && patch->m_Applied)
				patch->Restore();
		}
		m_Patches.clear();
		m_Datas.clear();
		m_ProgramAddrs.clear();
	}

	std::shared_ptr<ScriptPatches::Patch> ScriptPatches::AddPatchImpl(joaat_t script, ScriptPointer pointer, std::vector<std::uint8_t> patch)
	{
		std::lock_guard lock(m_Mutex);
		auto instance = std::make_shared<Patch>(script, std::move(pointer), std::move(patch));
		m_Patches.push_back(instance);
		return instance;
	}

	void ScriptPatches::RemovePatchImpl(const std::shared_ptr<Patch>& patch)
	{
		if (!patch)
			return;

		std::lock_guard lock(m_Mutex);
		patch->Disable();
		std::erase(m_Patches, patch);
	}

	void ScriptPatches::TickImpl()
	{
		std::lock_guard lock(m_Mutex);
		if (!Pointers.Initialized || !Pointers.ScriptPrograms || !g_Memory)
			return;

		bool anyEnabled = false;
		for (const auto& patch : m_Patches)
		{
			if (patch && patch->IsEnabled())
			{
				anyEnabled = true;
				break;
			}
		}

		if (!anyEnabled)
			return;

		const auto now = std::chrono::steady_clock::now();
		if (now - m_LastRefresh < std::chrono::milliseconds(750))
			return;

		RefreshProgramsImpl(false);

		for (auto& patch : m_Patches)
		{
			if (patch && patch->IsEnabled())
				patch->Apply();
		}
	}

	void ScriptPatches::ResetImpl()
	{
		std::lock_guard lock(m_Mutex);
		for (auto& patch : m_Patches)
		{
			if (patch)
				patch->ResetRemoteState();
		}
		m_Datas.clear();
		m_ProgramAddrs.clear();
		m_LastRefresh = {};
	}

	void ScriptPatches::RefreshProgramsImpl(bool force)
	{
		if (!Pointers.Initialized || !Pointers.ScriptPrograms || !g_Memory)
			return;

		const auto now = std::chrono::steady_clock::now();
		if (!force && (now - m_LastRefresh < std::chrono::milliseconds(750)))
			return;

		m_LastRefresh = now;

		std::array<std::uintptr_t, 176> programPtrs{};
		if (!g_Memory->ReadBuffer(Pointers.ScriptPrograms, programPtrs.data(), sizeof(programPtrs), false))
			return;

		struct RemoteScrProgramBuffer
		{
			alignas(rage::scrProgram) std::array<std::uint8_t, sizeof(rage::scrProgram)> bytes{};
			const rage::scrProgram& AsProgram() const
			{
				return *reinterpret_cast<const rage::scrProgram*>(bytes.data());
			}
		};

		std::unordered_map<joaat_t, std::pair<std::uintptr_t, RemoteScrProgramBuffer>> activePrograms;
		for (const auto ptr : programPtrs)
		{
			if (!IMemory::IsValidUserAddress(ptr))
				continue;

			RemoteScrProgramBuffer progBuf{};
			if (!g_Memory->ReadBuffer(ptr, progBuf.bytes.data(), progBuf.bytes.size(), false))
				continue;

			const auto& prog = progBuf.AsProgram();
			if (!prog.m_CodeBlocks || !prog.m_CodeSize)
				continue;

			bool tracked = false;
			for (const auto& patch : m_Patches)
			{
				if (patch && patch->InScope(prog.m_NameHash))
				{
					tracked = true;
					break;
				}
			}

			if (tracked)
				activePrograms.emplace(prog.m_NameHash, std::make_pair(ptr, progBuf));
		}

		// Drop unloaded scripts
		for (auto it = m_ProgramAddrs.begin(); it != m_ProgramAddrs.end();)
		{
			const auto hash = it->first;
			if (!activePrograms.contains(hash))
			{
				m_Datas.erase(hash);
				for (auto& patch : m_Patches)
				{
					if (patch && patch->InScope(hash))
						patch->ResetRemoteState();
				}
				it = m_ProgramAddrs.erase(it);
			}
			else
			{
				++it;
			}
		}

		// Register or reload changed scripts
		for (const auto& [hash, entry] : activePrograms)
		{
			const auto& [addr, progBuf] = entry;
			const auto existingIt = m_ProgramAddrs.find(hash);
			if (existingIt != m_ProgramAddrs.end() && existingIt->second == addr && m_Datas.contains(hash))
				continue;

			auto data = std::make_unique<ScriptData>(addr, progBuf.AsProgram());
			if (data->IsValid())
			{
				m_Datas[hash] = std::move(data);
				m_ProgramAddrs[hash] = addr;
			}
		}
	}

	ScriptData* ScriptPatches::GetDataImpl(joaat_t script)
	{
		if (auto it = m_Datas.find(script); it != m_Datas.end())
			return it->second.get();

		return nullptr;
	}

	std::uintptr_t ScriptPatches::GetProgramAddrImpl(joaat_t script)
	{
		if (auto it = m_ProgramAddrs.find(script); it != m_ProgramAddrs.end())
			return it->second;

		return 0;
	}
}
