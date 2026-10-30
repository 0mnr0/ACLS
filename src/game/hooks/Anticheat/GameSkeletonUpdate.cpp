#include "game/hooks/Hooks.hpp"
#include "core/util/Joaat.hpp"
#include "types/rage/gameSkeleton.hpp"
#include <unordered_set>

namespace YimMenu::Hooks
{
	static bool RunNodeSEH(rage::gameSkeletonUpdateBase* node, unsigned long& out_code)
	{
		__try
		{
			node->Run();
			return true;
		}
		__except (out_code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	static void SafeRunNode(rage::gameSkeletonUpdateBase* node)
	{
		if (!node)
			return;

		unsigned long exception_code = 0;
		if (!RunNodeSEH(node, exception_code))
		{
			static std::unordered_set<uint32_t> reported_hashes;
			if (!reported_hashes.contains(node->m_Hash))
			{
				reported_hashes.insert(node->m_Hash);
				LOGF(WARNING, "GameSkeletonUpdate: Exception 0x{:08X} in node 0x{:08X} (ptr: {:p})", exception_code, node->m_Hash, (void*)node);
				Logger::FlushQueue();
			}
		}
	}

	void Anticheat::GameSkeletonUpdate(rage::gameSkeleton* skeleton, int type)
	{
		for (auto mode = skeleton->m_UpdateModes; mode; mode = mode->m_Next)
		{
			if (mode->m_Type != type)
				continue;

			for (auto group = mode->m_Head; group; group = group->m_Next)
			{
				if (group->m_Hash != "Common Main"_J)
				{
					SafeRunNode(group);
					continue;
				}

				for (auto item = static_cast<rage::gameSkeletonUpdateGroup*>(group)->m_Head; item; item = item->m_Next)
				{
					if (item->m_Hash != 0xA0F39FB6 && item->m_Hash != "TamperActions"_J)
					{
						SafeRunNode(item);
					}
				}
			}

			break;
		}
	}
}
