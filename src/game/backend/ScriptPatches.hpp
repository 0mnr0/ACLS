#pragma once
#include "core/util/Joaat.hpp"
#include "game/gta/ScriptData.hpp"
#include "game/gta/ScriptPointer.hpp"
#include <chrono>
#include <mutex>

namespace YimMenu
{
	class ScriptPatches
	{
	public:
		class Patch
		{
			std::vector<std::uint8_t> m_OriginalBytes;
			std::vector<std::uint8_t> m_PatchedBytes;
			std::optional<std::uint32_t> m_Pc;
			ScriptPointer m_Pointer;
			bool m_Enabled{false};
			bool m_Applied{false};
			std::uintptr_t m_AppliedProgramAddr{0};
			std::uint32_t m_Hash{0};

			std::optional<std::uint32_t> GetPC();
			void Apply();
			void Restore();
			void ResetRemoteState();

			friend class ScriptPatches;

		public:
			Patch(joaat_t script, ScriptPointer pointer, std::vector<std::uint8_t> patch);
			void Enable();
			void Disable();
			void Update();
			bool InScope(std::uint32_t hash) const;
			bool IsEnabled() const { return m_Enabled; }
			bool IsApplied() const { return m_Applied; }
			joaat_t GetScriptHash() const { return m_Hash; }
		};

		static std::shared_ptr<Patch> AddPatch(joaat_t script, ScriptPointer pointer, std::vector<std::uint8_t> patch)
		{
			return GetInstance().AddPatchImpl(script, std::move(pointer), std::move(patch));
		}

		static void RemovePatch(const std::shared_ptr<Patch>& patch)
		{
			GetInstance().RemovePatchImpl(patch);
		}

		static void Tick()
		{
			GetInstance().TickImpl();
		}

		static void Reset()
		{
			GetInstance().ResetImpl();
		}

	private:
		ScriptPatches() = default;
		~ScriptPatches();

		static ScriptPatches& GetInstance()
		{
			static ScriptPatches Instance;
			return Instance;
		}

		std::shared_ptr<Patch> AddPatchImpl(joaat_t script, ScriptPointer pointer, std::vector<std::uint8_t> patch);
		void RemovePatchImpl(const std::shared_ptr<Patch>& patch);
		void TickImpl();
		void ResetImpl();
		void RefreshProgramsImpl(bool force);
		ScriptData* GetDataImpl(joaat_t script);
		std::uintptr_t GetProgramAddrImpl(joaat_t script);

		std::recursive_mutex m_Mutex;
		std::vector<std::shared_ptr<Patch>> m_Patches;
		std::unordered_map<joaat_t, std::unique_ptr<ScriptData>> m_Datas;
		std::unordered_map<joaat_t, std::uintptr_t> m_ProgramAddrs;
		std::chrono::steady_clock::time_point m_LastRefresh{};
	};

	using ScriptPatch = std::shared_ptr<ScriptPatches::Patch>;
}
