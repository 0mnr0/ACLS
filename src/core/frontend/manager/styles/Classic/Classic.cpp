#include "game/frontend/Menu.hpp"
#include "core/frontend/manager/UIManager.hpp"
#include "core/frontend/manager/styles/Themes.hpp"

namespace YimMenu
{
	void RenderClassicTheme()
	{
		const auto& displaySize = ImGui::GetIO().DisplaySize;
		const ImVec2 defaultPadding = ImGui::GetStyle().WindowPadding;

		constexpr float titleBarHeight = 38.0f;
		constexpr float padding = 10.0f;
		constexpr float sidebarWidth = 155.0f;
		constexpr float categoryBarHeight = 50.0f;

		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(displaySize, ImGuiCond_Always);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
		    | ImGuiWindowFlags_NoResize
		    | ImGuiWindowFlags_NoMove
		    | ImGuiWindowFlags_NoCollapse
		    | ImGuiWindowFlags_NoBringToFrontOnFocus
		    | ImGuiWindowFlags_NoScrollbar
		    | ImGuiWindowFlags_NoScrollWithMouse
		    | ImGuiWindowFlags_NoSavedSettings;

		if (ImGui::Begin("##MainWindow", nullptr, flags))
		{
			DrawCustomTitleBar(titleBarHeight);

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, defaultPadding);

			const float bodyTop = titleBarHeight + padding;
			const float bodyHeight = std::max(1.0f, displaySize.y - bodyTop - padding);
			const float rightX = padding + sidebarWidth + padding;
			const float rightWidth = std::max(1.0f, displaySize.x - rightX - padding);

			ImGui::SetCursorPos(ImVec2(padding, bodyTop));
			if (ImGui::BeginChild("##submenus", ImVec2(sidebarWidth, bodyHeight), true, ImGuiWindowFlags_NoTitleBar))
			{
				const auto& submenus = YimMenu::UIManager::GetSubmenus();
				auto activeSubmenu = YimMenu::UIManager::GetActiveSubmenu();

				for (auto& submenu : submenus)
				{
					const bool isActive = (submenu == activeSubmenu);
					const ImVec2 itemPos = ImGui::GetCursorScreenPos();
					const float itemWidth = ImGui::GetContentRegionAvail().x;
					constexpr float itemHeight = 34.0f;

					if (isActive)
					{
						ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.50f, 0.30f, 0.45f));
						ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.35f, 0.59f, 0.35f, 0.55f));
					}

					if (ImGui::Selectable(("##submenu_" + submenu->m_Name).c_str(), isActive, 0, ImVec2(itemWidth, itemHeight)))
					{
						YimMenu::UIManager::SetActiveSubmenu(submenu);
						YimMenu::UIManager::SetShowContentWindow(true);
					}

					if (isActive)
					{
						ImGui::PopStyleColor(2);
						ImGui::GetWindowDrawList()->AddRectFilled(
						    ImVec2(itemPos.x, itemPos.y + 5.0f),
						    ImVec2(itemPos.x + 3.0f, itemPos.y + itemHeight - 5.0f),
						    IM_COL32(92, 217, 143, 255),
						    1.5f);
					}

					ImDrawList* childDraw = ImGui::GetWindowDrawList();
					const float textCenterY = itemPos.y + itemHeight * 0.5f;
					float textX = itemPos.x + 12.0f;

					if (Menu::Font::g_AwesomeFont && !submenu->m_Icon.empty())
					{
						constexpr float iconSize = 15.0f;
						const ImVec2 measuredIcon = Menu::Font::g_AwesomeFont->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, submenu->m_Icon.c_str());
						childDraw->AddText(
						    Menu::Font::g_AwesomeFont,
						    iconSize,
						    ImVec2(textX, textCenterY - measuredIcon.y * 0.5f),
						    isActive ? IM_COL32(130, 230, 165, 255) : IM_COL32(180, 185, 195, 255),
						    submenu->m_Icon.c_str());
						textX += 24.0f;
					}

					const ImVec2 labelSize = ImGui::CalcTextSize(submenu->m_Name.c_str());
					childDraw->AddText(
					    ImVec2(textX, textCenterY - labelSize.y * 0.5f),
					    isActive ? IM_COL32(240, 244, 248, 255) : IM_COL32(205, 210, 218, 255),
					    submenu->m_Name.c_str());
				}
			}
			ImGui::EndChild();

			ImGui::SetCursorPos(ImVec2(rightX, bodyTop));
			if (ImGui::BeginChild("##minisubmenus", ImVec2(rightWidth, categoryBarHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
			{
				auto activeSubmenu = YimMenu::UIManager::GetActiveSubmenu();
				if (activeSubmenu)
					activeSubmenu->DrawCategorySelectors();
			}
			ImGui::EndChild();

			const float optionsTop = bodyTop + categoryBarHeight + padding;
			const float optionsHeight = std::max(1.0f, displaySize.y - optionsTop - padding);
			ImGui::SetCursorPos(ImVec2(rightX, optionsTop));
			if (ImGui::BeginChild("##options", ImVec2(rightWidth, optionsHeight), true))
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

			ImGui::PopStyleVar();
		}
		ImGui::End();

		ImGui::PopStyleVar(3);
	}
}
