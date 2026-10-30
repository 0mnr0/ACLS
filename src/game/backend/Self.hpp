#pragma once
#include "game/gta/Entity.hpp"
#include "game/gta/Ped.hpp"
#include "game/gta/Vehicle.hpp"

namespace YimMenu
{
	class Self
	{
		Ped m_SelfPed;
		Vehicle m_Vehicle;

	public:
		static Ped GetPed()
		{
			return GetInstance().m_SelfPed;
		}

		static Vehicle GetVehicle()
		{
			return GetInstance().m_Vehicle;
		}

		static void RunScript()
		{
			GetInstance().RunScriptImpl();
		}

		static void Update()
		{
			GetInstance().UpdateImpl();
		}

	private:
		Self();
		void RunScriptImpl();
		void UpdateImpl();

		static Self& GetInstance()
		{
			static Self Instance;
			return Instance;
		}
	};
}
