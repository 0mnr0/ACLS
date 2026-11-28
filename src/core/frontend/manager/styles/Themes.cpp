#include "Themes.hpp"
#include "core/commands/ColorCommand.hpp"
#include "core/renderer/Renderer.hpp"
#include "game/backend/DmaSession.hpp"

namespace YimMenu
{
	void DrawCustomTitleBar(float height)
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImVec2 winPos = ImGui::GetWindowPos();
		const float winWidth = ImGui::GetWindowWidth();
		const ImVec2 barMin = winPos;
		const ImVec2 barMax(winPos.x + winWidth, winPos.y + height);

		// Top bar background and bottom separator
		drawList->AddRectFilled(barMin, barMax, IM_COL32(18, 19, 23, 255));
		drawList->AddLine(ImVec2(barMin.x, barMax.y - 1.0f), ImVec2(barMax.x, barMax.y - 1.0f), IM_COL32(45, 48, 56, 255), 1.0f);

		// Left branding & DMA status
		const float centerY = barMin.y + height * 0.5f;
		float cursorX = barMin.x + 14.0f;

		drawList->AddRectFilled(ImVec2(cursorX, centerY - 6.0f), ImVec2(cursorX + 3.0f, centerY + 6.0f), IM_COL32(92, 217, 143, 255), 1.5f);
		cursorX += 11.0f;

		const char* title = "SafeYimV2";
		const ImVec2 titleSize = ImGui::CalcTextSize(title);
		drawList->AddText(ImVec2(cursorX, centerY - titleSize.y * 0.5f), IM_COL32(236, 239, 244, 255), title);
		cursorX += titleSize.x + 8.0f;

		const char* badge = "DMA";
		const ImVec2 badgeSize = ImGui::CalcTextSize(badge);
		const ImVec2 badgeMin(cursorX, centerY - badgeSize.y * 0.5f - 2.0f);
		const ImVec2 badgeMax(cursorX + badgeSize.x + 10.0f, centerY + badgeSize.y * 0.5f + 2.0f);
		drawList->AddRectFilled(badgeMin, badgeMax, IM_COL32(42, 82, 52, 180), 4.0f);
		drawList->AddRect(badgeMin, badgeMax, IM_COL32(92, 217, 143, 140), 4.0f);
		drawList->AddText(ImVec2(badgeMin.x + 5.0f, centerY - badgeSize.y * 0.5f), IM_COL32(130, 230, 165, 255), badge);
		cursorX = badgeMax.x + 14.0f;

		ImU32 statusDotColor = IM_COL32(130, 134, 142, 255);
		const char* statusText = "Disconnected";
		switch (DmaSession::GetState())
		{
		case DmaState::Connected:
			statusDotColor = IM_COL32(76, 217, 102, 255);
			statusText = "Connected";
			break;
		case DmaState::Connecting:
			statusDotColor = IM_COL32(242, 191, 51, 255);
			statusText = "Connecting...";
			break;
		case DmaState::Error:
			statusDotColor = IM_COL32(242, 89, 89, 255);
			statusText = "Error";
			break;
		case DmaState::Disconnected:
		default:
			break;
		}

		drawList->AddCircleFilled(ImVec2(cursorX + 4.0f, centerY), 3.5f, statusDotColor);
		cursorX += 13.0f;
		const ImVec2 statusSize = ImGui::CalcTextSize(statusText);
		drawList->AddText(ImVec2(cursorX, centerY - statusSize.y * 0.5f), IM_COL32(165, 170, 180, 255), statusText);

		// Right-side window control buttons (Minimize, Maximize/Restore, Close)
		constexpr float buttonWidth = 46.0f;
		constexpr float buttonsWidth = buttonWidth * 3.0f;
		const float buttonsStartX = barMax.x - buttonsWidth;
		bool anyButtonHovered = false;

		auto drawControlButton = [&](const char* id, int index, bool isClose, auto&& drawGlyph, auto&& onClick) {
			const ImVec2 btnMin(buttonsStartX + index * buttonWidth, barMin.y);
			const ImVec2 btnMax(btnMin.x + buttonWidth, barMax.y - 1.0f);
			const ImVec2 btnCenter((btnMin.x + btnMax.x) * 0.5f, (btnMin.y + btnMax.y) * 0.5f);

			ImGui::SetCursorScreenPos(btnMin);
			ImGui::InvisibleButton(id, ImVec2(buttonWidth, height - 1.0f));

			const bool hovered = ImGui::IsItemHovered();
			const bool active = ImGui::IsItemActive();
			anyButtonHovered |= hovered;

			if (hovered || active)
			{
				ImU32 bgCol = isClose
				    ? (active ? IM_COL32(168, 35, 22, 255) : IM_COL32(196, 43, 28, 255))
				    : (active ? IM_COL32(255, 255, 255, 36) : IM_COL32(255, 255, 255, 22));
				drawList->AddRectFilled(btnMin, btnMax, bgCol);
			}

			const ImU32 iconCol = (isClose && hovered) ? IM_COL32(255, 255, 255, 255) : IM_COL32(215, 218, 224, 255);
			drawGlyph(btnCenter, iconCol);

			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
				onClick();
		};

		drawControlButton(
		    "##TitleBarMinimize",
		    0,
		    false,
		    [&](const ImVec2& c, ImU32 col) {
			    drawList->AddLine(ImVec2(c.x - 5.0f, c.y), ImVec2(c.x + 5.0f, c.y), col, 1.2f);
		    },
		    [] { Renderer::MinimizeWindow(); });

		drawControlButton(
		    "##TitleBarMaximize",
		    1,
		    false,
		    [&](const ImVec2& c, ImU32 col) {
			    if (Renderer::IsWindowMaximized())
			    {
				    drawList->AddRect(ImVec2(c.x - 3.0f, c.y - 5.0f), ImVec2(c.x + 5.0f, c.y + 3.0f), col, 0.0f, 0, 1.2f);
				    drawList->AddRectFilled(ImVec2(c.x - 5.0f, c.y - 3.0f), ImVec2(c.x + 3.0f, c.y + 5.0f), IM_COL32(18, 19, 23, 255));
				    drawList->AddRect(ImVec2(c.x - 5.0f, c.y - 3.0f), ImVec2(c.x + 3.0f, c.y + 5.0f), col, 0.0f, 0, 1.2f);
			    }
			    else
			    {
				    drawList->AddRect(ImVec2(c.x - 4.5f, c.y - 4.5f), ImVec2(c.x + 4.5f, c.y + 4.5f), col, 0.0f, 0, 1.2f);
			    }
		    },
		    [] { Renderer::ToggleMaximizeWindow(); });

		drawControlButton(
		    "##TitleBarClose",
		    2,
		    true,
		    [&](const ImVec2& c, ImU32 col) {
			    drawList->AddLine(ImVec2(c.x - 4.5f, c.y - 4.5f), ImVec2(c.x + 4.5f, c.y + 4.5f), col, 1.3f);
			    drawList->AddLine(ImVec2(c.x + 4.5f, c.y - 4.5f), ImVec2(c.x - 4.5f, c.y + 4.5f), col, 1.3f);
		    },
		    [] { Renderer::CloseWindow(); });

		Renderer::SetTitleBarInfo(height, buttonsWidth, anyButtonHovered);

		ImGui::SetCursorScreenPos(ImVec2(winPos.x, winPos.y + height));
	}

	void DrawCustomTitleBarWindow(float height)
	{
		const auto& displaySize = ImGui::GetIO().DisplaySize;
		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(displaySize.x, height), ImGuiCond_Always);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		if (ImGui::Begin("##TopBarWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings))
		{
			DrawCustomTitleBar(height);
		}
		ImGui::End();
		ImGui::PopStyleVar(3);
	}

	void DefaultStyle()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		// Text
		style.Colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
		style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

		// Backgrounds
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
		style.Colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
		style.Colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.09f, 0.95f);

		// Borders
		style.Colors[ImGuiCol_Border] = ImVec4(0.28f, 0.28f, 0.28f, 0.25f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

		// Frames
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.35f, 0.59f, 0.35f, 0.75f);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.47f, 0.75f, 0.47f, 0.85f);

		// Title bars
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.06f, 0.07f, 1.00f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.04f, 0.04f, 0.05f, 1.00f);

		// Menus/tabs
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
		style.Colors[ImGuiCol_Tab] = ImVec4(0.30f, 0.55f, 0.30f, 0.85f);
		style.Colors[ImGuiCol_TabHovered] = ImVec4(0.40f, 0.65f, 0.40f, 0.85f);
		style.Colors[ImGuiCol_TabActive] = ImVec4(0.47f, 0.75f, 0.47f, 1.00f);
		style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.25f, 0.40f, 0.25f, 0.70f);
		style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.30f, 0.55f, 0.30f, 0.85f);

		// Scrollbar
		style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.80f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

		// Checkmarks, sliders
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.36f, 0.85f, 0.56f, 1.00f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.59f, 0.35f, 1.00f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.47f, 0.75f, 0.47f, 1.00f);

		// Buttons
		style.Colors[ImGuiCol_Button] = ImVec4(0.35f, 0.59f, 0.35f, 1.00f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.40f, 0.65f, 0.40f, 1.00f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.47f, 0.75f, 0.47f, 1.00f);

		// Headers
		style.Colors[ImGuiCol_Header] = ImVec4(0.30f, 0.50f, 0.30f, 1.00f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.40f, 0.65f, 0.40f, 1.00f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.47f, 0.75f, 0.47f, 1.00f);

		// Separators
		style.Colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.50f);
		style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.45f, 0.80f, 0.45f, 0.78f);
		style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.55f, 0.90f, 0.55f, 1.00f);

		// Resize grips
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.50f, 0.50f, 0.50f, 0.30f);
		style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.60f, 0.85f, 0.60f, 0.60f);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.70f, 0.95f, 0.70f, 0.90f);

		// Plots
		style.Colors[ImGuiCol_PlotLines] = ImVec4(0.80f, 0.80f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.85f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.35f, 0.59f, 0.35f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.47f, 0.75f, 0.47f, 1.00f);

		// Drag & drop
		style.Colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);

		// Navigation highlighting
		style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.40f, 0.90f, 0.40f, 1.00f);
		style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
		style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);

		// Modal/background dimming
		style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.35f);

		// Rounding
		style.GrabRounding = style.FrameRounding = style.ChildRounding = style.WindowRounding = 8.0f;
	}

	void SetupStyle()
	{
		DefaultStyle();
	}
}
