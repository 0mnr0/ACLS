#include "Renderer.hpp"
#include "core/frontend/manager/styles/Themes.hpp"
#include "game/frontend/Menu.hpp"
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#include <dwmapi.h>
#include <imgui.h>
#include <windowsx.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace YimMenu
{
	bool Renderer::InitImpl()
	{
		if (m_Initialized)
			return true;

		m_WindowClass = {
		    sizeof(WNDCLASSEXW),
		    CS_CLASSDC,
		    &Renderer::WndProc,
		    0L,
		    0L,
		    GetModuleHandleW(nullptr),
		    nullptr,
		    LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)),
		    nullptr,
		    nullptr,
		    L"SafeYimV2DMAWindowClass",
		    nullptr};

		if (!RegisterClassExW(&m_WindowClass))
		{
			LOG(FATAL) << "Failed to register window class.";
			return false;
		}

		m_Window = CreateWindowW(
		    m_WindowClass.lpszClassName,
		    L"SafeYimV2 DMA",
		    WS_OVERLAPPEDWINDOW,
		    100,
		    100,
		    1024,
		    680,
		    nullptr,
		    nullptr,
		    m_WindowClass.hInstance,
		    nullptr);

		if (!m_Window)
		{
			UnregisterClassW(m_WindowClass.lpszClassName, m_WindowClass.hInstance);
			LOG(FATAL) << "Failed to create application window.";
			return false;
		}

		constexpr MARGINS shadowMargins{0, 0, 0, 1};
		DwmExtendFrameIntoClientArea(m_Window, &shadowMargins);

		BOOL darkMode = TRUE;
		DwmSetWindowAttribute(m_Window, 20, &darkMode, sizeof(darkMode));
		DWORD cornerPreference = 2; // DWMWCP_ROUND on Windows 11
		DwmSetWindowAttribute(m_Window, 33, &cornerPreference, sizeof(cornerPreference));

		SetWindowPos(m_Window, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

		if (!CreateDeviceD3D(m_Window))
		{
			CleanupDeviceD3D();
			DestroyWindow(m_Window);
			m_Window = nullptr;
			UnregisterClassW(m_WindowClass.lpszClassName, m_WindowClass.hInstance);
			LOG(FATAL) << "Failed to create D3D11 device.";
			return false;
		}

		ShowWindow(m_Window, SW_SHOWDEFAULT);
		UpdateWindow(m_Window);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		ImGui::StyleColorsDark();

		ImGui_ImplWin32_Init(m_Window);
		ImGui_ImplDX11_Init(m_Device.Get(), m_DeviceContext.Get());

		Menu::SetupFonts();
		SetupStyle();

		m_Initialized = true;
		LOG(INFO) << "Standalone DX11 Renderer initialized.";
		return true;
	}

	void Renderer::DestroyImpl()
	{
		if (!m_Initialized)
			return;

		m_Initialized = false;
		m_SafeToRender = false;

		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		CleanupDeviceD3D();

		if (m_Window)
		{
			DestroyWindow(m_Window);
			m_Window = nullptr;
		}

		if (m_WindowClass.lpszClassName)
		{
			UnregisterClassW(m_WindowClass.lpszClassName, m_WindowClass.hInstance);
			m_WindowClass = {};
		}
	}

	bool Renderer::TickImpl()
	{
		if (!m_Initialized)
			return false;

		MSG msg{};
		while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
			if (msg.message == WM_QUIT)
			{
				g_Running = false;
				return false;
			}
		}

		if (!g_Running)
			return false;

		HandleResize();

		Render();
		return true;
	}

	void Renderer::HandleResize()
	{
		if (m_ResizeWidth == 0 || m_ResizeHeight == 0 || !m_SwapChain)
			return;

		CleanupRenderTarget();
		m_SwapChain->ResizeBuffers(0, m_ResizeWidth, m_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
		m_ResizeWidth = 0;
		m_ResizeHeight = 0;
		CreateRenderTarget();
	}

	void Renderer::Render()
	{
		if (!m_SafeToRender || !m_RenderTargetView || m_IsRendering)
			return;

		m_IsRendering = true;

		if (m_FontsUpdated)
		{
			ImGui_ImplDX11_InvalidateDeviceObjects();
			ImGui_ImplDX11_CreateDeviceObjects();
			m_FontsUpdated = false;
		}

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		for (const auto& callback : m_RendererCallbacks | std::views::values)
			callback();

		ImGui::Render();

		constexpr float clearColor[4] = {0.059f, 0.059f, 0.059f, 1.0f};
		ID3D11RenderTargetView* rtv = m_RenderTargetView.Get();
		m_DeviceContext->OMSetRenderTargets(1, &rtv, nullptr);
		m_DeviceContext->ClearRenderTargetView(rtv, clearColor);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		m_SwapChain->Present(1, 0);
		m_IsRendering = false;
	}

	bool Renderer::CreateDeviceD3D(HWND hWnd)
	{
		DXGI_SWAP_CHAIN_DESC sd{};
		sd.BufferCount = 2;
		sd.BufferDesc.Width = 0;
		sd.BufferDesc.Height = 0;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		UINT createDeviceFlags = 0;
		D3D_FEATURE_LEVEL featureLevel{};
		const D3D_FEATURE_LEVEL featureLevelArray[2] = {
		    D3D_FEATURE_LEVEL_11_0,
		    D3D_FEATURE_LEVEL_10_0,
		};

		HRESULT res = D3D11CreateDeviceAndSwapChain(
		    nullptr,
		    D3D_DRIVER_TYPE_HARDWARE,
		    nullptr,
		    createDeviceFlags,
		    featureLevelArray,
		    2,
		    D3D11_SDK_VERSION,
		    &sd,
		    &m_SwapChain,
		    &m_Device,
		    &featureLevel,
		    &m_DeviceContext);

		if (res == DXGI_ERROR_UNSUPPORTED)
		{
			res = D3D11CreateDeviceAndSwapChain(
			    nullptr,
			    D3D_DRIVER_TYPE_WARP,
			    nullptr,
			    createDeviceFlags,
			    featureLevelArray,
			    2,
			    D3D11_SDK_VERSION,
			    &sd,
			    &m_SwapChain,
			    &m_Device,
			    &featureLevel,
			    &m_DeviceContext);
		}

		if (FAILED(res))
			return false;

		CreateRenderTarget();
		return true;
	}

	void Renderer::CleanupDeviceD3D()
	{
		CleanupRenderTarget();
		m_SwapChain.Reset();
		m_DeviceContext.Reset();
		m_Device.Reset();
	}

	void Renderer::CreateRenderTarget()
	{
		ComPtr<ID3D11Texture2D> backBuffer{};
		if (SUCCEEDED(m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) && backBuffer)
		{
			m_Device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_RenderTargetView);
		}
	}

	void Renderer::CleanupRenderTarget()
	{
		m_RenderTargetView.Reset();
	}

	LRESULT CALLBACK Renderer::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
			return true;

		if (GetInstance().m_Initialized)
		{
			for (const auto& callback : GetInstance().m_WindowProcedureCallbacks)
				callback(hWnd, msg, wParam, lParam);
		}

		switch (msg)
		{
		case WM_NCCALCSIZE:
			if (wParam == TRUE)
			{
				auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
				if (IsZoomed(hWnd))
				{
					HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
					MONITORINFO mi{sizeof(MONITORINFO)};
					if (GetMonitorInfoW(monitor, &mi))
						params->rgrc[0] = mi.rcWork;
				}
				return 0;
			}
			break;
		case WM_GETMINMAXINFO:
		{
			auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
			mmi->ptMinTrackSize.x = 640;
			mmi->ptMinTrackSize.y = 440;
			return 0;
		}
		case WM_ERASEBKGND:
			return 1;
		case WM_NCHITTEST:
		{
			POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
			RECT winRect{};
			GetWindowRect(hWnd, &winRect);

			if (!IsZoomed(hWnd))
			{
				constexpr int borderWidth = 5;
				const bool left = pt.x < winRect.left + borderWidth;
				const bool right = pt.x >= winRect.right - borderWidth;
				const bool top = pt.y < winRect.top + borderWidth;
				const bool bottom = pt.y >= winRect.bottom - borderWidth;

				if (top && left)
					return HTTOPLEFT;
				if (top && right)
					return HTTOPRIGHT;
				if (bottom && left)
					return HTBOTTOMLEFT;
				if (bottom && right)
					return HTBOTTOMRIGHT;
				if (left)
					return HTLEFT;
				if (right)
					return HTRIGHT;
				if (top)
					return HTTOP;
				if (bottom)
					return HTBOTTOM;
			}

			POINT clientPt = pt;
			ScreenToClient(hWnd, &clientPt);
			RECT clientRect{};
			GetClientRect(hWnd, &clientRect);
			const int clientWidth = clientRect.right - clientRect.left;

			auto& inst = GetInstance();
			if (clientPt.y >= 0
			    && clientPt.y < static_cast<LONG>(inst.m_TitleBarHeight)
			    && clientPt.x >= 0
			    && clientPt.x < clientWidth - static_cast<LONG>(inst.m_TitleBarControlsWidth)
			    && !inst.m_TitleBarItemHovered)
			{
				return HTCAPTION;
			}

			return HTCLIENT;
		}
		case WM_SIZE:
			if (wParam == SIZE_MINIMIZED)
				return 0;
			GetInstance().m_ResizeWidth = LOWORD(lParam);
			GetInstance().m_ResizeHeight = HIWORD(lParam);
			if (GetInstance().m_Initialized && GetInstance().m_SafeToRender && !GetInstance().m_IsRendering
			    && GetInstance().m_ResizeWidth > 0 && GetInstance().m_ResizeHeight > 0)
			{
				GetInstance().HandleResize();
				GetInstance().Render();
			}
			return 0;
		case WM_SYSCOMMAND:
			if ((wParam & 0xfff0) == SC_KEYMENU)
				return 0;
			break;
		case WM_DESTROY:
			g_Running = false;
			PostQuitMessage(0);
			return 0;
		}

		return DefWindowProcW(hWnd, msg, wParam, lParam);
	}
}
