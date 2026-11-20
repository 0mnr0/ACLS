#include "Vehicle.hpp"
#include "core/commands/Commands.hpp"
#include "core/commands/IntCommand.hpp"
#include "game/frontend/items/Items.hpp"
#include "core/dma/IMemory.hpp"
#include "game/features/debug/NativeDiagnostics.hpp"

#include <cstring>
#include <format>
#include <string>

namespace YimMenu::Submenus
{
	Vehicle::Vehicle() :
		#define ICON_FA_CAR "\xef\x86\xb9"
	    Submenu::Submenu("Vehicle", ICON_FA_CAR)
	{
		auto main    = std::make_shared<Category>("Main");
		auto globals = std::make_shared<Group>("Globals");

		// --- Ghost Mode (local physics, single-player style) ---
		globals->AddItem(std::make_shared<BoolCommandItem>("ghostmode"_J, "Ghost Mode"));
		globals->AddItem(std::make_shared<ConditionalItem>("ghostmode"_J,
		    std::make_shared<IntCommandItem>("ghostmodealpha"_J, "Alpha")));

		// --- Network Ghost Mode (physics flags + netObject ghost flag) ---
		globals->AddItem(std::make_shared<BoolCommandItem>("networkghostmode"_J, "Network Ghost Mode"));
		globals->AddItem(std::make_shared<ConditionalItem>("networkghostmode"_J,
		    std::make_shared<BoolCommandItem>("networkghostmodifyalpha"_J, "Modify Alpha")));
		globals->AddItem(std::make_shared<ConditionalItem>("networkghostmodifyalpha"_J,
		    std::make_shared<IntCommandItem>("networkghostalpha"_J, "Alpha")));
		globals->AddItem(std::make_shared<ConditionalItem>("networkghostmode"_J, std::make_shared<ImGuiItem>([] {
			ImGui::TextDisabled("Applies physics ghost flags, alpha and netObject ghost flag (+0x1C5)");
		})));

		// --- DMA write test: harmless bytes in the DOS stub of the main module's PE header ---
		globals->AddItem(std::make_shared<ImGuiItem>([] {
			static constexpr std::ptrdiff_t k_Off = 0x50; // inside "This program cannot be run in DOS mode" stub
			static constexpr std::size_t k_Len = 16;
			static std::uint8_t s_Orig[k_Len]{};
			static bool s_Have = false;
			static std::string s_Status = "idle";

			auto getAddr = []() -> std::uintptr_t {
				RemoteModuleInfo mod;
				if (!g_Memory || !g_Memory->IsProcessAttached() || !g_Memory->GetModule(g_Memory->GetProcessName(), mod))
					return 0;
				return mod.base + k_Off;
			};

			if (ImGui::Button("DMA write test"))
			{
				const auto addr = getAddr();
				std::uint8_t pattern[k_Len], back[k_Len]{};
				for (std::size_t i = 0; i < k_Len; ++i)
					pattern[i] = static_cast<std::uint8_t>(0xA0 + i);
				if (!addr)
					s_Status = "module not found";
				else if (!s_Have && !g_Memory->ReadBuffer(addr, s_Orig, k_Len))
					s_Status = "read failed";
				else
				{
					s_Have = true;
					const bool w = g_Memory->WriteBuffer(addr, pattern, k_Len);
					const bool r = g_Memory->ReadBuffer(addr, back, k_Len);
					s_Status = std::format("addr 0x{:X}: write={} verify={}", addr, w, r && std::memcmp(pattern, back, k_Len) == 0);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Restore") && s_Have)
			{
				const auto addr = getAddr();
				s_Status = (addr && g_Memory->WriteBuffer(addr, s_Orig, k_Len)) ? "restored" : "restore failed";
				s_Have = false;
			}
			ImGui::TextDisabled("%s", s_Status.c_str());
		}));

		globals->AddItem(std::make_shared<ImGuiItem>([] { DrawNativeDiagnostics(); }));

		main->AddItem(globals);
		AddCategory(std::move(main));
	}
}
