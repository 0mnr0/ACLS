#include "Menu.hpp"
#include "core/frontend/manager/UIManager.hpp"
#include "core/renderer/Renderer.hpp"
#include "game/frontend/fonts/Fonts.hpp"
#include "submenus/Vehicle.hpp"
#include "submenus/Other.hpp"
#include "Onboarding.hpp"

namespace YimMenu
{
	void Menu::Init()
	{
		// Arguably the only place this file should be edited at for more menus
		UIManager::AddSubmenu(std::make_shared<Submenus::Vehicle>());
		UIManager::AddSubmenu(std::make_shared<Submenus::Other>());

		Renderer::AddRendererCallback(
		    [&] {
			    ProcessOnboarding();
			    if (!GUI::IsOpen())
				    return;

			    ImGui::PushFont(Menu::Font::g_DefaultFont);
			    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImU32(ImColor(15, 15, 15)));

			    UIManager::Draw();

			    ImGui::PopStyleColor();
			    ImGui::PopFont();
		    },
		    -1);
	}

	static const ImWchar* GetGlyphRangesCyrillicOnly()
	{
		static const ImWchar ranges[] =
		    {
		        0x0400,
		        0x052F, // Cyrillic + Cyrillic Supplement
		        0x2DE0,
		        0x2DFF, // Cyrillic Extended-A
		        0xA640,
		        0xA69F, // Cyrillic Extended-B
		        0,
		    };
		return &ranges[0];
	}

	static ImFont* CreateFontWithCyrillicSupport(ImGuiIO& io, float size)
	{
		ImFontConfig FontCfg{};
		FontCfg.FontDataOwnedByAtlas = false;

		auto font = io.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(Fonts::MainFont), sizeof(Fonts::MainFont), size, &FontCfg, io.Fonts->GetGlyphRangesDefault());

		// just use Arial for Cyrillic

		FontCfg.MergeMode = true;
		
		const auto fonts_dir = std::filesystem::path(std::getenv("SYSTEMROOT")) / "Fonts";
		if (const auto arial = fonts_dir / "arial.ttf"; std::filesystem::exists(arial))
			io.Fonts->AddFontFromFileTTF(arial.string().c_str(), size, &FontCfg, GetGlyphRangesCyrillicOnly());
		if (const auto meiryo = fonts_dir / "meiryo.ttc"; std::filesystem::exists(meiryo))
			io.Fonts->AddFontFromFileTTF(meiryo.string().c_str(), size, &FontCfg, io.Fonts->GetGlyphRangesJapanese());
		
		return font;
	}

	void Menu::SetupFonts()
	{
		auto& IO = ImGui::GetIO();
		auto file_path = std::filesystem::path(std::getenv("appdata")) / "YimMenuV2" / "imgui.ini";
		static auto path = file_path.string();
		IO.IniFilename = path.c_str();
		IO.LogFilename = NULL;
		ImFontConfig FontCfg{};
		FontCfg.FontDataOwnedByAtlas = false;

		IO.Fonts->Clear();
		Menu::Font::g_DefaultFont = CreateFontWithCyrillicSupport(IO, Menu::Font::g_DefaultFontSize);
		Menu::Font::g_OptionsFont = CreateFontWithCyrillicSupport(IO, Menu::Font::g_OptionsFontSize);
		Menu::Font::g_ChildTitleFont = CreateFontWithCyrillicSupport(IO, Menu::Font::g_ChildTitleFontSize);
		Menu::Font::g_ChatFont = CreateFontWithCyrillicSupport(IO, Menu::Font::g_ChatFontSize);
		Menu::Font::g_OverlayFont = CreateFontWithCyrillicSupport(IO, Menu::Font::g_OverlayFontSize);
		static const ImWchar full_range[] = {0x0020, 0xFFFF, 0};
		Menu::Font::g_AwesomeFont = IO.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(Fonts::IconFont), sizeof(Fonts::IconFont), Menu::Font::g_AwesomeFontSize, &FontCfg, full_range);
		IO.Fonts->Build();

		UIManager::SetOptionsFont(Menu::Font::g_OptionsFont);
		Renderer::SetFontsUpdated();
	}
}
