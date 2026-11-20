#include "DMA.hpp"
#include "core/commands/Commands.hpp"
#include "core/commands/IntCommand.hpp"
#include "game/backend/DmaSession.hpp"
#include "game/backend/Self.hpp"
#include "game/frontend/items/Items.hpp"
#include "game/pointers/Pointers.hpp"

namespace YimMenu::Submenus
{
	DMA::DMA() :
	    #define ICON_FA_SERVER "\xef\x88\xb3"
	    Submenu::Submenu("DMA", ICON_FA_SERVER)
	{
		auto connection = std::make_shared<Category>("Connection");
		auto statusGroup = std::make_shared<Group>("Status", 1);
		auto settingsGroup = std::make_shared<Group>("Settings", 1);
		auto offsetsGroup = std::make_shared<Group>("Offsets", 1);

		statusGroup->AddItem(std::make_shared<ImGuiItem>([] {
			const auto state = DmaSession::GetState();
			const auto msg = DmaSession::GetStatusMessage();

			ImVec4 color{0.7f, 0.7f, 0.7f, 1.0f};
			const char* label = "Disconnected";
			switch (state)
			{
			case DmaState::Connected:
				color = ImVec4(0.3f, 0.85f, 0.4f, 1.0f);
				label = "Connected";
				break;
			case DmaState::Connecting:
				color = ImVec4(0.95f, 0.75f, 0.2f, 1.0f);
				label = "Connecting";
				break;
			case DmaState::Error:
				color = ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
				label = "Error";
				break;
			case DmaState::Disconnected:
			default:
				break;
			}

			ImGui::TextColored(color, "State: %s", label);
			ImGui::TextWrapped("%s", msg.c_str());

			if (state == DmaState::Connected)
			{
				ImGui::Separator();
				ImGui::Text("PID: %u", DmaSession::GetPid());
				if (!DmaSession::GetGameVersion().empty())
					ImGui::Text("Game Version: %s (Online %s)", DmaSession::GetGameVersion().c_str(), DmaSession::GetOnlineVersion().c_str());
				ImGui::Text("Local Ped: 0x%llX", static_cast<unsigned long long>(Self::GetPedPtr()));
				ImGui::Text("Vehicle Ptr: 0x%llX", static_cast<unsigned long long>(Self::GetVehiclePtr()));
				ImGui::Text("Player ID: %u", static_cast<unsigned int>(Self::GetPlayerId()));
				ImGui::Text("PedFactory: 0x%llX", static_cast<unsigned long long>(Pointers.PedFactory));
				ImGui::Text("NetworkPlayerMgr: 0x%llX", static_cast<unsigned long long>(Pointers.NetworkPlayerMgr));
				ImGui::Text("ScriptGlobals: 0x%llX", static_cast<unsigned long long>(Pointers.ScriptGlobals));
			}
		}));

		settingsGroup->AddItem(std::make_shared<StringCommandItem>("dmadevice"_J));
		settingsGroup->AddItem(std::make_shared<StringCommandItem>("dmaprocess"_J));
		settingsGroup->AddItem(std::make_shared<BoolCommandItem>("dmaautoconnect"_J));
		settingsGroup->AddItem(std::make_shared<CommandItem>("dmaconnect"_J));
		settingsGroup->AddItem(std::make_shared<CommandItem>("dmadisconnect"_J));

		offsetsGroup->AddItem(std::make_shared<ImGuiItem>([] {
			static char offsetBuffer[32] = "";
			auto cmd = Commands::GetCommand<IntCommand>("dmavehoffset"_J);
			if (!cmd)
				return;

			int currentVal = cmd->GetState();
			if (!ImGui::IsItemActive())
			{
				snprintf(offsetBuffer, sizeof(offsetBuffer), "0x%X", currentVal);
			}

			ImGui::SetNextItemWidth(150);
			if (ImGui::InputText("Ped->Vehicle Offset", offsetBuffer, sizeof(offsetBuffer)))
			{
				std::string str = offsetBuffer;
				while (!str.empty() && (str.front() == ' ' || str.front() == '\t'))
					str.erase(str.begin());
				while (!str.empty() && (str.back() == ' ' || str.back() == '\t'))
					str.pop_back();

				if (!str.empty())
				{
					try
					{
						int parsed = 0;
						if (str.starts_with("0x") || str.starts_with("0X"))
							parsed = std::stoi(str, nullptr, 16);
						else if (str.find_first_of("abcdefABCDEF") != std::string::npos)
							parsed = std::stoi(str, nullptr, 16);
						else
							parsed = std::stoi(str, nullptr, 10);

						if (parsed >= 0 && parsed <= 0x10000)
							cmd->SetState(parsed);
					}
					catch (...)
					{
					}
				}
			}
			ImGui::SameLine();
			ImGui::TextDisabled("(dec: %d, hex: 0x%X)", currentVal, currentVal);
		}));

		connection->AddItem(std::move(statusGroup));
		connection->AddItem(std::move(settingsGroup));
		connection->AddItem(std::move(offsetsGroup));
		AddCategory(std::move(connection));
	}
}
