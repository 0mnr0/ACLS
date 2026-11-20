#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace YimMenu
{
	struct RemoteModuleInfo
	{
		std::string name{};
		std::string fullPath{};
		std::uintptr_t base{0};
		std::uintptr_t size{0};
	};

	class IScatterHandle
	{
	public:
		virtual ~IScatterHandle() = default;

		virtual bool PrepareRead(std::uintptr_t address, void* outBuffer, std::size_t size) = 0;
		virtual bool PrepareWrite(std::uintptr_t address, const void* data, std::size_t size) = 0;
		virtual bool Execute() = 0;
		virtual bool Clear() = 0;

		template<typename T>
		bool PrepareRead(std::uintptr_t address, T& outValue)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			return PrepareRead(address, &outValue, sizeof(T));
		}

		template<typename T>
		bool PrepareWrite(std::uintptr_t address, const T& value)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			return PrepareWrite(address, &value, sizeof(T));
		}
	};

	class IMemory
	{
	public:
		virtual ~IMemory() = default;

		virtual bool Initialize(std::string_view deviceArg, std::string_view processName) = 0;
		virtual void Shutdown() = 0;

		virtual bool IsConnected() const = 0;
		virtual bool IsProcessAttached() const = 0;
		virtual bool AttachProcess(std::string_view processName) = 0;

		virtual std::uint32_t GetPid() const = 0;
		virtual std::string_view GetProcessName() const = 0;
		virtual std::string_view GetLastError() const = 0;

		virtual bool ReadBuffer(std::uintptr_t address, void* buffer, std::size_t size, bool noCache = true) = 0;
		virtual bool WriteBuffer(std::uintptr_t address, const void* buffer, std::size_t size) = 0;

		virtual bool GetModule(std::string_view moduleName, RemoteModuleInfo& outInfo) = 0;
		virtual std::vector<RemoteModuleInfo> GetModules() = 0;
		virtual std::unique_ptr<IScatterHandle> CreateScatterHandle(bool noCache = true) = 0;

		static constexpr bool IsValidUserAddress(std::uintptr_t address)
		{
			return address >= 0x10000 && address <= 0x00007FFFFFFFFFFFULL;
		}

		template<typename T>
		bool Read(std::uintptr_t address, T& outValue, bool noCache = true)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			if (!IsValidUserAddress(address))
				return false;
			return ReadBuffer(address, &outValue, sizeof(T), noCache);
		}

		template<typename T>
		std::optional<T> Read(std::uintptr_t address, bool noCache = true)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			T value{};
			if (!Read(address, value, noCache))
				return std::nullopt;
			return value;
		}

		template<typename T>
		bool Write(std::uintptr_t address, const T& value)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			if (!IsValidUserAddress(address))
				return false;
			return WriteBuffer(address, &value, sizeof(T));
		}

		template<typename T>
		std::optional<T> ReadChain(std::uintptr_t base, std::span<const std::uintptr_t> offsets, bool noCache = true)
		{
			std::uintptr_t current = base;
			for (std::size_t i = 0; i < offsets.size(); ++i)
			{
				if (!IsValidUserAddress(current))
					return std::nullopt;

				if (i + 1 < offsets.size())
				{
					auto next = Read<std::uintptr_t>(current + offsets[i], noCache);
					if (!next.has_value())
						return std::nullopt;
					current = *next;
				}
				else
				{
					return Read<T>(current + offsets[i], noCache);
				}
			}
			return std::nullopt;
		}
	};

	inline std::unique_ptr<IMemory> g_Memory{};
}
