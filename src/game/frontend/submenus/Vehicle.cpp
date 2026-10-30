#include "Vehicle.hpp"
#include "game/frontend/items/Items.hpp"

namespace YimMenu::Submenus
{
	Vehicle::Vehicle() :
		#define ICON_FA_CAR "\xef\x86\xb9"
	    Submenu::Submenu("Vehicle", ICON_FA_CAR)
	{
		auto main = std::make_shared<Category>("Main");
		auto globals = std::make_shared<Group>("Globals");

		globals->AddItem(std::make_shared<BoolCommandItem>("ghostmode"_J, "Ghost Mode"));
		globals->AddItem(std::make_shared<ConditionalItem>("ghostmode"_J, std::make_shared<IntCommandItem>("ghostmodealpha"_J, "Alpha")));

		main->AddItem(globals);
		AddCategory(std::move(main));
	}
}
