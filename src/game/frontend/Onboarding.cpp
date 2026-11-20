#include "Onboarding.hpp"
#include "GUI.hpp"
#include "core/commands/BoolCommand.hpp"
#include "core/commands/Commands.hpp"
#include <shellapi.h>

namespace YimMenu
{
	static BoolCommand _OnboardingComplete{"$onboardingcomplete", "", ""};

	void ProcessOnboarding()
	{
		if (_OnboardingComplete.GetState())
			return;

		static bool ensure_popup_open = [] {
			ImGui::OpenPopup("SafeYimV2 DMA");
			GUI::SetOnboarding(true);
			return true;
		}();

		const auto& displaySize = ImGui::GetIO().DisplaySize;
		const auto window_size = ImVec2{580, 320};
		const auto window_position = ImVec2{(displaySize.x - window_size.x) / 2.0f, (displaySize.y - window_size.y) / 2.0f};

		ImGui::SetNextWindowSize(window_size, ImGuiCond_Once);
		ImGui::SetNextWindowPos(window_position, ImGuiCond_Once);

		if (ImGui::BeginPopupModal("SafeYimV2 DMA", nullptr, ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::TextWrapped("%s",
			    "Welcome to SafeYimV2 DMA Edition! This external application communicates with GTA5_Enhanced.exe over a PCIe DMA device using MemProcFS (vmm.dll / leechcore.dll).");
			ImGui::Spacing();
			ImGui::TextWrapped("%s",
			    "Place vmm.dll, leechcore.dll, and FTD3XX.dll next to the executable, launch GTA5_Enhanced.exe on the gaming PC, and use the DMA tab to connect or configure device options.");
			ImGui::Spacing();
			ImGui::TextWrapped("%s",
			    "Note: Features requiring internal code hooks or native invocation are omitted in DMA mode; only direct memory features are active.");
			ImGui::Spacing();

			if (ImGui::Button("Open GitHub"))
			{
				ShellExecuteA(NULL, "open", "https://github.com/0mnr0/ACLS", NULL, NULL, SW_SHOWNORMAL);
			}
			ImGui::SameLine();
			if (ImGui::Button("Close"))
			{
				_OnboardingComplete.SetState(true);
				GUI::SetOnboarding(false);
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}
}
