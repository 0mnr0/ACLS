#include "core/commands/LoopedCommand.hpp"
#include "core/dma/IMemory.hpp"
#include "game/gta/ScriptGlobal.hpp"
#include "game/pointers/Pointers.hpp"

namespace YimMenu::Features
{
	class SilencePhonecalls : public LoopedCommand
	{
		using LoopedCommand::LoopedCommand;

		virtual void OnTick() override
		{
			if (!Pointers.Initialized || !g_Memory || !Pointers.IsSessionStarted)
				return;

			const auto isSessionStarted = g_Memory->Read<bool>(Pointers.IsSessionStarted);
			if (!isSessionStarted.has_value() || !*isSessionStarted)
				return;

			constexpr auto is_incoming_call = ScriptGlobal(23050);
			constexpr auto is_phone_call_in_progress = ScriptGlobal(23046);
			constexpr auto phone_call_state = ScriptGlobal(23040);
			constexpr auto calling_character = ScriptGlobal(8818);

			const auto callState = phone_call_state.Read<int>();
			const auto inProgress = is_phone_call_in_progress.Read<bool>();
			const auto incoming = is_incoming_call.Read<bool>();

			if (callState.has_value() && inProgress.has_value() && incoming.has_value())
			{
				if (*callState != 0 && *callState != 5 && *callState != 6 && *inProgress && *incoming)
				{
					const auto characterId = calling_character.Read<int>().value_or(-1);
					LOGF(VERBOSE, "SilencePhoneCalls::OnTick(): Skipped phone call from character {}", characterId);
					phone_call_state.Write<int>(6);
				}
			}
		}
	};

	static SilencePhonecalls _SilencePhonecalls{"nocalls", "Silence Phone Calls", "Automatically silences all incoming calls"};
}
