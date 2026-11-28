#include "GUI.hpp"
#include "Menu.hpp"
#include "core/frontend/Notifications.hpp"
#include "core/frontend/manager/UIManager.hpp"
#include "core/renderer/Renderer.hpp"

namespace YimMenu
{
	void GUI::InitImpl()
	{
		Menu::Init();

		Renderer::AddWindowProcedureCallback([this](HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
			GUI::WndProc(hwnd, msg, wparam, lparam);
		});

		Renderer::AddRendererCallback(
		    [&] {
			    Notifications::Draw();
		    },
		    10);

		Renderer::SetSafeToRender();
	}

	void GUI::ToggleMouse()
	{
		if (!ImGui::GetCurrentContext())
			return;

		auto& io = ImGui::GetIO();
		io.MouseDrawCursor = false;
		io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
	}

	void GUI::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (msg == WM_KEYUP
		    && (wparam == VK_INSERT || (wparam == VK_OEM_5 && (GetKeyState(VK_CONTROL) & 0x8000) != 0)))
		{
			if (!GUI::IsOnboarding())
				Toggle();
		}
	}

	void GUI::SetOnboardingImpl(bool state)
	{
		m_Onboarding = state;
		ToggleMouse();
	}
}
