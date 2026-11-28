#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace YimMenu
{
	class Renderer
	{
	public:
		using RendererCallback = std::function<void()>;
		using WindowProcedureCallback = std::function<void(HWND, UINT, WPARAM, LPARAM)>;

		static bool Init()
		{
			return GetInstance().InitImpl();
		}

		static void Destroy()
		{
			GetInstance().DestroyImpl();
		}

		static bool Tick()
		{
			return GetInstance().TickImpl();
		}

		static void AddRendererCallback(RendererCallback&& callback, uint32_t priority)
		{
			GetInstance().m_RendererCallbacks.insert({priority, std::move(callback)});
		}

		static void AddWindowProcedureCallback(WindowProcedureCallback&& callback)
		{
			GetInstance().m_WindowProcedureCallbacks.push_back(std::move(callback));
		}

		static bool IsInitialized()
		{
			return GetInstance().m_Initialized;
		}

		static void SetSafeToRender()
		{
			GetInstance().m_SafeToRender = true;
		}

		static void SetFontsUpdated()
		{
			GetInstance().m_FontsUpdated = true;
		}

		static HWND GetWindow()
		{
			return GetInstance().m_Window;
		}

		static void SetTitleBarInfo(float height, float controlsWidth, bool itemHovered)
		{
			auto& inst = GetInstance();
			inst.m_TitleBarHeight = height;
			inst.m_TitleBarControlsWidth = controlsWidth;
			inst.m_TitleBarItemHovered = itemHovered;
		}

		static void MinimizeWindow()
		{
			if (auto hwnd = GetInstance().m_Window)
				ShowWindow(hwnd, SW_MINIMIZE);
		}

		static void ToggleMaximizeWindow()
		{
			if (auto hwnd = GetInstance().m_Window)
				ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
		}

		static void CloseWindow()
		{
			if (auto hwnd = GetInstance().m_Window)
				PostMessageW(hwnd, WM_CLOSE, 0, 0);
		}

		static bool IsWindowMaximized()
		{
			auto hwnd = GetInstance().m_Window;
			return hwnd && IsZoomed(hwnd);
		}

	private:
		template<typename T>
		using ComPtr = Microsoft::WRL::ComPtr<T>;

		static Renderer& GetInstance()
		{
			static Renderer instance;
			return instance;
		}

		bool InitImpl();
		void DestroyImpl();
		bool TickImpl();
		void Render();
		void HandleResize();

		bool CreateDeviceD3D(HWND hWnd);
		void CleanupDeviceD3D();
		void CreateRenderTarget();
		void CleanupRenderTarget();

		static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

		bool m_Initialized = false;
		bool m_SafeToRender = false;
		bool m_FontsUpdated = false;
		bool m_IsRendering = false;
		HWND m_Window = nullptr;
		WNDCLASSEXW m_WindowClass{};
		UINT m_ResizeWidth = 0;
		UINT m_ResizeHeight = 0;
		float m_TitleBarHeight = 38.0f;
		float m_TitleBarControlsWidth = 138.0f;
		bool m_TitleBarItemHovered = false;

		ComPtr<ID3D11Device> m_Device{};
		ComPtr<ID3D11DeviceContext> m_DeviceContext{};
		ComPtr<IDXGISwapChain> m_SwapChain{};
		ComPtr<ID3D11RenderTargetView> m_RenderTargetView{};

		std::map<uint32_t, RendererCallback> m_RendererCallbacks;
		std::vector<WindowProcedureCallback> m_WindowProcedureCallbacks;
	};
}
