#include "Other.hpp"
#include "game/frontend/items/Items.hpp"

namespace YimMenu::Submenus
{
	Other::Other() :
		#define ICON_FA_GLOBE "\xef\x82\xac"
	    Submenu::Submenu("Other", ICON_FA_GLOBE)
	{
		auto main = std::make_shared<Category>("Main");
		auto otherOpts = std::make_shared<Group>("Other", 1);

		otherOpts->AddItem(std::make_shared<BoolCommandItem>("bypassbejoin"_J));
		otherOpts->AddItem(std::make_shared<BoolCommandItem>("enablecreatordevmode"_J));
		otherOpts->AddItem(std::make_shared<BoolCommandItem>("nocalls"_J));

		main->AddItem(std::move(otherOpts));
		AddCategory(std::move(main));
	}
}
