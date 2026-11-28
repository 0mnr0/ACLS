#include "core/commands/LoopedCommand.hpp"
#include "core/dma/IMemory.hpp"
#include "game/backend/Self.hpp"
#include "game/pointers/Pointers.hpp"

namespace YimMenu::Features
{
	struct Vector3Float
	{
		float x, y, z;
	};

	static constexpr std::ptrdiff_t k_PedCamSubOffset   = 0x1060;
	static constexpr std::ptrdiff_t k_PedPosOffset      = 0x0090;
	static constexpr std::ptrdiff_t k_CamViewModeOffset = 0x00C4;
	static constexpr std::ptrdiff_t k_CamPosOffset      = 0x00E0;

	class ExtendedCamera : public LoopedCommand
	{
		using LoopedCommand::LoopedCommand;

		virtual void OnTick() override
		{
			if (!Pointers.Initialized || !g_Memory)
				return;

			const auto ped = Self::GetPedPtr();
			if (!ped || !IMemory::IsValidUserAddress(ped))
				return;

			const auto camObj = g_Memory->Read<std::uintptr_t>(ped + k_PedCamSubOffset).value_or(0);
			if (!camObj || !IMemory::IsValidUserAddress(camObj))
				return;

			// Camera view mode:
			// 0 = Near, 1 = Medium, 2 = Far, 4 = First Person
			const auto viewMode = g_Memory->Read<std::int32_t>(camObj + k_CamViewModeOffset).value_or(-1);
			if (viewMode < 0 || viewMode > 2)
				return;

			const auto pedPos = g_Memory->Read<Vector3Float>(ped + k_PedPosOffset);
			const auto camPos = g_Memory->Read<Vector3Float>(camObj + k_CamPosOffset);
			if (!pedPos.has_value() || !camPos.has_value())
				return;

			const float dx = camPos->x - pedPos->x;
			const float dy = camPos->y - pedPos->y;
			const float dz = camPos->z - pedPos->z;

			const float distSq = dx * dx + dy * dy + dz * dz;
			// Sanity check distance: only apply if camera is within sensible 3rd-person range (0.5m .. 30m)
			if (distSq < 0.25f || distSq > 900.0f)
				return;

			// Double the distance: Ped + 2.0 * (Cam - Ped)
			Vector3Float target;
			target.x = pedPos->x + 5.0f * dx;
			target.y = pedPos->y + 5.0f * dy;
			target.z = pedPos->z + 5.0f * dz;

			g_Memory->Write<Vector3Float>(camObj + k_CamPosOffset, target);
		}
	};

	static ExtendedCamera _ExtendedCamera{"extendedcamera", "Extended Camera Distance",
	    "Doubles third-person camera distance for close, medium, and far views"};
}
