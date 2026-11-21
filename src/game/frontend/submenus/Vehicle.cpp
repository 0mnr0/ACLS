#include "Vehicle.hpp"
#include "core/commands/Commands.hpp"
#include "core/commands/IntCommand.hpp"
#include "game/frontend/items/Items.hpp"


namespace YimMenu::Submenus
{
	Vehicle::Vehicle() :
		#define ICON_FA_CAR "\xef\x86\xb9"
	    Submenu::Submenu("Vehicle", ICON_FA_CAR)
	{
		auto main    = std::make_shared<Category>("Main");
		auto globals = std::make_shared<Group>("Globals");

		// --- Network Ghost Mode (netObject ghost flag) ---
		globals->AddItem(std::make_shared<BoolCommandItem>("networkghostmode"_J, "Network Ghost Mode"));
		// globals->AddItem(std::make_shared<ConditionalItem>("networkghostmode"_J,
		//     std::make_shared<BoolCommandItem>("networkghostmodifyalpha"_J, "Modify Alpha")));
		// globals->AddItem(std::make_shared<ConditionalItem>("networkghostmodifyalpha"_J,
		//     std::make_shared<IntCommandItem>("networkghostalpha"_J, "Alpha")));

		main->AddItem(globals);
		AddCategory(std::move(main));
	}
}
