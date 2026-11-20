#include "game/frontend/Menu.hpp"
#include "core/frontend/manager/UIManager.hpp"

namespace YimMenu
{
	void RenderClassicTheme()
	{
		const auto& displaySize = ImGui::GetIO().DisplaySize;
		float windowWidth = std::max(560.0f, displaySize.x * 0.75f);
		float windowHeight = std::max(380.0f, displaySize.y * 0.75f);
		float centerX = (displaySize.x - windowWidth) / 2.0f;
		float centerY = (displaySize.y - windowHeight) / 2.0f;
		ImVec2 windowSize(windowWidth, windowHeight);

		ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(centerX, centerY), ImGuiCond_FirstUseEver);

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse;
		auto pos = ImGui::GetCursorPos();
		if(ImGui::Begin("##ClassicInputWindow", nullptr, flags))
		{
			if (ImGui::BeginChild("##submenus", ImVec2(120, ImGui::GetContentRegionAvail().y - 20), true, ImGuiWindowFlags_NoTitleBar))
			{
				const auto& submenus = YimMenu::UIManager::GetSubmenus();
				auto activeSubmenu = YimMenu::UIManager::GetActiveSubmenu();

				for (auto& submenu : submenus)
				{
					if (ImGui::Selectable(submenu->m_Name.data(), (submenu == activeSubmenu)))
					{
						YimMenu::UIManager::SetActiveSubmenu(submenu);
						YimMenu::UIManager::SetShowContentWindow(true);
					}
				}
			}
			ImGui::EndChild();

			ImGui::Text("SafeYimV2");

			pos.y -= 28;
			ImGui::SetCursorPos(ImVec2(pos.x + 130, pos.y));

			if (ImGui::BeginChild("##minisubmenus", ImVec2(0, 50), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
			{
				auto activeSubmenu = YimMenu::UIManager::GetActiveSubmenu();
				if (activeSubmenu)
					activeSubmenu->DrawCategorySelectors();
			}
			ImGui::EndChild();

			ImGui::SetCursorPos(ImVec2(pos.x + 130, pos.y + 60));

			if (ImGui::BeginChild("##options", ImVec2(0, 0), true))
			{
				auto optionsFont = YimMenu::UIManager::GetOptionsFont();
				if (optionsFont)
					ImGui::PushFont(optionsFont);

				auto activeSubmenu = YimMenu::UIManager::GetActiveSubmenu();
				if (activeSubmenu)
					activeSubmenu->Draw();

				if (optionsFont)
					ImGui::PopFont();
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}
}
