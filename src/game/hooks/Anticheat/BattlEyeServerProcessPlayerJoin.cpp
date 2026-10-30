#include "core/hooking/DetourHook.hpp"
#include "core/commands/BoolCommand.hpp"
#include "core/commands/Commands.hpp"
#include "core/util/Joaat.hpp"
#include "game/hooks/Hooks.hpp"
#include "game/gta/Natives.hpp"
#include "types/battleye/CBattlEyePlayerModifyContext.hpp"
#include "game/backend/AnticheatBypass.hpp"

namespace YimMenu::Features
{
	static BoolCommand _BypassBattlEyeServerProcessPlayerJoin{
	    "bypassbejoin",
	    "Bypass BattlEye Player Join",
	    "Forces BattlEyeServerProcessPlayerJoin to return true (prevents disconnects and solo lobby when running without BattlEye)",
	    true};
}

namespace YimMenu::Hooks
{
	bool Anticheat::BattlEyeServerProcessPlayerJoin(CBattlEyePlayerModifyInterface* server_iface, CBattlEyePlayerModifyContext* context)
	{
		if (!AnticheatBypass::IsBattlEyeRunning())
		{
			if (context->m_IsLocal)
			{
				if (NETWORK::NETWORK_SESSION_IS_CLOSED_FRIENDS() || NETWORK::NETWORK_SESSION_IS_CLOSED_CREW() || NETWORK::NETWORK_SESSION_IS_SOLO() || NETWORK::NETWORK_SESSION_IS_PRIVATE())
				{
					return true;
				}
			}

			static auto bypass_be_join = Commands::GetCommand<BoolCommand>("bypassbejoin"_J);
			if (bypass_be_join && bypass_be_join->GetState())
			{
				return true;
			}
		}

		return BaseHook::Get<Anticheat::BattlEyeServerProcessPlayerJoin, DetourHook<decltype(&Anticheat::BattlEyeServerProcessPlayerJoin)>>()->Original()(server_iface, context);
	}
}
