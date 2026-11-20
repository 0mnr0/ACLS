#include "PatternScanner.hpp"

#include "Module.hpp"
#include "core/backend/PatternCache.hpp"

#include <future>

namespace YimMenu
{
	PatternScanner::PatternScanner(const Module* module) :
	    m_Module(module),
	    m_Patterns()
	{
	}

	bool PatternScanner::Scan()
	{
		if (!m_Module || !m_Module->Valid())
			return false;

		// Check if every pattern is already in PatternCache before dumping the full remote module
		bool needsMemoryDump = false;
		for (const auto& [pattern, func] : m_Patterns)
		{
			if (!PatternCache::IsInitialized() || !PatternCache::GetCachedOffset(pattern->Hash().Update(m_Module->Size())).has_value())
			{
				needsMemoryDump = true;
				break;
			}
		}

		if (needsMemoryDump)
		{
			auto* mutableMod = const_cast<Module*>(m_Module);
			if (!mutableMod->CacheImage())
			{
				LOG(WARNING) << "Failed to cache remote image for " << m_Module->Name();
				return false;
			}
		}

		bool scanSuccess = true;
		std::vector<std::future<bool>> jobs;
		jobs.reserve(m_Patterns.size());

		for (const auto& [pattern, func] : m_Patterns)
		{
			jobs.emplace_back(std::async(std::launch::async, &PatternScanner::ScanInternal, this, pattern, func));
		}

		for (auto& job : jobs)
		{
			const bool ok = job.get();
			scanSuccess = scanSuccess && ok;
		}

		return scanSuccess;
	}

	bool PatternScanner::ScanInternal(const IPattern* pattern, PatternFunc func) const
	{
		const auto signature = pattern->Signature();

		if (PatternCache::IsInitialized())
		{
			auto offset = PatternCache::GetCachedOffset(pattern->Hash().Update(m_Module->Size()));
			if (offset.has_value())
			{
				const std::uintptr_t resolved = m_Module->Base() + offset.value();
				LOGF(INFO, "Using cached pattern [{}] : [{:X}] [Hash(): {:X}]", pattern->Name(), resolved, pattern->Hash().Update(m_Module->Size()).m_Hash);
				std::invoke(func, PointerCalculator(resolved));
				return true;
			}
		}

		const auto& image = m_Module->GetCachedImage();
		if (image.size() < signature.size())
		{
			LOG(WARNING) << "Failed to scan pattern [" << pattern->Name() << "]: module image cache unavailable";
			return false;
		}

		const std::size_t limit = image.size() - signature.size();
		for (std::size_t offset = 0; offset <= limit; ++offset)
		{
			bool found = true;
			for (std::size_t idx = 0; idx < signature.size(); ++idx)
			{
				if (signature[idx] && signature[idx].value() != image[offset + idx])
				{
					found = false;
					break;
				}
			}

			if (found)
			{
				const std::uintptr_t remoteAddr = m_Module->Base() + offset;
				LOG(INFO) << "Found pattern [" << pattern->Name() << "] : [" << HEX(remoteAddr) << "]";

				std::invoke(func, PointerCalculator(remoteAddr));

				if (PatternCache::IsInitialized())
				{
					PatternCache::UpdateCachedOffset(pattern->Hash().Update(m_Module->Size()), static_cast<int>(offset));
				}

				return true;
			}
		}

		LOG(WARNING) << "Failed to find pattern [" << pattern->Name() << "]";
		return false;
	}
}
