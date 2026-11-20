#include "NativeDiagnostics.hpp"

#include "common.hpp"
#include "core/dma/IMemory.hpp"
#include "core/memory/Module.hpp"
#include "core/memory/ModuleMgr.hpp"
#include "core/memory/Pattern.hpp"
#include "core/util/Joaat.hpp"

#include "game/gta/ScriptData.hpp"
#include "game/pointers/Pointers.hpp"
#include "types/script/scrProgram.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <cstdio>
#include <cstring>
#include <format>
#include <mutex>
#include <string>

// All functions in this file are READ-ONLY with respect to the game process.

namespace YimMenu
{
	namespace
	{
		std::mutex g_OutMutex;
		std::string g_Output;
		std::atomic<bool> g_Busy{false};

		void Out(const std::string& line)
		{
			LOG(INFO) << "[NativeDiag] " << line;
			std::lock_guard lk(g_OutMutex);
			g_Output += line;
			g_Output += '\n';
		}

		Module* GetGameModule()
		{
			return ModuleMgr.Get("GTA5_Enhanced.exe"_J);
		}

		std::string HexDump(std::uintptr_t addr, std::size_t len)
		{
			std::vector<std::uint8_t> buf(len);
			if (!g_Memory || !g_Memory->ReadBuffer(addr, buf.data(), len, false))
				return std::format("read failed at 0x{:X}", addr);

			std::string s;
			for (std::size_t i = 0; i < len; i += 16)
			{
				s += std::format("{:016X}: ", addr + i);
				for (std::size_t j = 0; j < 16 && i + j < len; ++j)
					s += std::format("{:02X} ", buf[i + j]);
				s += '\n';
			}
			return s;
		}

		const IMAGE_NT_HEADERS64* GetNt(const std::vector<std::uint8_t>& img)
		{
			if (img.size() < 0x1000)
				return nullptr;
			const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(img.data());
			if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || static_cast<std::size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > img.size())
				return nullptr;
			const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(img.data() + dos->e_lfanew);
			return nt->Signature == IMAGE_NT_SIGNATURE ? nt : nullptr;
		}

		// Section list + executable code caves (runs of 0x00 / 0xCC inside executable sections)
		void RunSectionsAndCaves(std::size_t minCave)
		{
			auto* mod = GetGameModule();
			if (!mod || !mod->CacheImage())
			{
				Out("module not found / image cache failed");
				return;
			}
			const auto& img = mod->GetCachedImage();
			const auto* nt = GetNt(img);
			if (!nt)
			{
				Out("PE header invalid");
				return;
			}

			Out(std::format("Module base 0x{:X}, size 0x{:X}", mod->Base(), mod->Size()));
			const auto* sec = IMAGE_FIRST_SECTION(nt);

			struct Cave
			{
				std::uintptr_t rva;
				std::size_t len;
				std::string section;
				std::uint8_t fill;
			};
			std::vector<Cave> caves;

			for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i)
			{
				char name[9]{};
				std::memcpy(name, sec[i].Name, 8);
				const bool exec = (sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
				const bool write = (sec[i].Characteristics & IMAGE_SCN_MEM_WRITE) != 0;
				Out(std::format("  section {:<8} RVA 0x{:X} vsize 0x{:X} {}{}", name, sec[i].VirtualAddress, sec[i].Misc.VirtualSize, exec ? "X" : "-", write ? "W" : "-"));

				if (!exec)
					continue;

				const std::size_t begin = sec[i].VirtualAddress;
				const std::size_t end = std::min<std::size_t>(begin + sec[i].Misc.VirtualSize, img.size());
				for (std::size_t p = begin; p < end;)
				{
					const std::uint8_t b = img[p];
					if (b != 0x00 && b != 0xCC)
					{
						++p;
						continue;
					}
					std::size_t q = p;
					while (q < end && img[q] == b)
						++q;
					if (q - p >= minCave)
						caves.push_back({p, q - p, name, b});
					p = q;
				}
			}

			std::sort(caves.begin(), caves.end(), [](auto& a, auto& b) { return a.len > b.len; });
			Out(std::format("Code caves >= {} bytes in executable sections: {} found (top 10):", minCave, caves.size()));
			for (std::size_t i = 0; i < caves.size() && i < 10; ++i)
				Out(std::format("  {} RVA 0x{:X} (abs 0x{:X}) len 0x{:X} fill 0x{:02X}", caves[i].section, caves[i].rva, mod->Base() + caves[i].rva, caves[i].len, caves[i].fill));

			mod->ClearCache();
		}

		// Generic pattern scan + RIP resolve + hexdump. Used to locate the native registration table.
		void RunPatternDump(std::string patternStr, int ripOffset, int dumpLen)
		{
			auto* mod = GetGameModule();
			if (!mod || !mod->CacheImage())
			{
				Out("module not found / image cache failed");
				return;
			}
			const auto& img = mod->GetCachedImage();
			SimplePattern pat(patternStr + " "); // ctor reads up to size-1
			const auto& bytes = pat.m_Bytes;
			if (bytes.empty() || img.size() < bytes.size())
			{
				Out("empty pattern");
				mod->ClearCache();
				return;
			}

			std::size_t hits = 0;
			for (std::size_t off = 0; off + bytes.size() <= img.size() && hits < 5; ++off)
			{
				bool ok = true;
				for (std::size_t k = 0; k < bytes.size(); ++k)
					if (bytes[k] && *bytes[k] != img[off + k])
					{
						ok = false;
						break;
					}
				if (!ok)
					continue;

				++hits;
				Out(std::format("Match #{} at RVA 0x{:X} (abs 0x{:X})", hits, off, mod->Base() + off));
				Out(HexDump(mod->Base() + off, 32));

				if (ripOffset >= 0 && off + ripOffset + 4 <= img.size())
				{
					std::int32_t rel;
					std::memcpy(&rel, &img[off + ripOffset], 4);
					const std::uintptr_t target = mod->Base() + off + ripOffset + 4 + rel;
					Out(std::format("RIP target (offset {}): 0x{:X} (RVA 0x{:X})", ripOffset, target, target - mod->Base()));
					Out(HexDump(target, std::clamp(dumpLen, 16, 512)));
					if (auto p = g_Memory->Read<std::uintptr_t>(target))
					{
						Out(std::format("qword at target = 0x{:X}", *p));
						if (IMemory::IsValidUserAddress(*p))
						{
							Out("dereferenced:");
							Out(HexDump(*p, std::clamp(dumpLen, 16, 512)));
						}
					}
				}
			}
			if (!hits)
				Out("pattern not found");

			mod->ClearCache();
		}

		void RunHexDump(std::uintptr_t addr, int len)
		{
			Out(HexDump(addr, std::clamp(len, 16, 1024)));
		}

		// ------------------------------------------------------------------
		// Script (bytecode) diagnostics
		// ------------------------------------------------------------------
		struct LoadedScript
		{
			std::uintptr_t addr{};
			std::string name;
			std::uint32_t nameHash{};
			std::uint32_t codeSize{};
			std::uint32_t nativeCount{};
			std::uintptr_t nativeTable{};
		};

		std::vector<LoadedScript> EnumerateScripts()
		{
			std::vector<LoadedScript> res;
			if (!g_Memory || !Pointers.Initialized || !Pointers.ScriptPrograms)
				return res;

			std::array<std::uintptr_t, 176> ptrs{};
			if (!g_Memory->ReadBuffer(Pointers.ScriptPrograms, ptrs.data(), sizeof(ptrs), false))
				return res;

			for (auto p : ptrs)
			{
				if (!IMemory::IsValidUserAddress(p))
					continue;
				alignas(rage::scrProgram) std::array<std::uint8_t, sizeof(rage::scrProgram)> buf{};
				if (!g_Memory->ReadBuffer(p, buf.data(), buf.size(), false))
					continue;
				const auto& prog = *reinterpret_cast<const rage::scrProgram*>(buf.data());
				if (!prog.m_CodeSize || !prog.m_CodeBlocks)
					continue;

				LoadedScript s;
				s.addr = p;
				s.nameHash = prog.m_NameHash;
				s.codeSize = prog.m_CodeSize;
				s.nativeCount = prog.m_NativeCount;
				s.nativeTable = reinterpret_cast<std::uintptr_t>(prog.m_NativeEntrypoints);
				char nm[64]{};
				if (auto np = reinterpret_cast<std::uintptr_t>(prog.m_Name); IMemory::IsValidUserAddress(np))
					g_Memory->ReadBuffer(np, nm, sizeof(nm) - 1, false);
				s.name = nm[0] ? nm : std::format("0x{:X}", s.nameHash);
				res.push_back(std::move(s));
			}
			return res;
		}

		bool LoadScriptCode(const std::string& name, LoadedScript& outScript, std::vector<std::uint8_t>& outCode)
		{
			auto lower = [](std::string s) {
				std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
				return s;
			};
			for (auto& s : EnumerateScripts())
			{
				if (lower(s.name) != lower(name))
					continue;

				alignas(rage::scrProgram) std::array<std::uint8_t, sizeof(rage::scrProgram)> buf{};
				if (!g_Memory->ReadBuffer(s.addr, buf.data(), buf.size(), false))
					return false;
				ScriptData data(s.addr, *reinterpret_cast<const rage::scrProgram*>(buf.data()));
				if (!data.IsValid())
				{
					Out("ScriptData invalid for " + s.name);
					return false;
				}
				outCode.assign(s.codeSize, 0);
				for (std::uint32_t pc = 0; pc < s.codeSize; pc += 0x4000)
				{
					auto* loc = data.GetCodeLocation(pc);
					if (!loc)
						return false;
					std::memcpy(outCode.data() + pc, loc, std::min<std::uint32_t>(0x4000, s.codeSize - pc));
				}
				outScript = s;
				return true;
			}
			Out("script not running: " + name);
			return false;
		}

		void RunScriptList()
		{
			const auto scripts = EnumerateScripts();
			Out(std::format("Loaded scripts: {}", scripts.size()));
			for (auto& s : scripts)
				Out(std::format("  {:<32} hash 0x{:08X} code 0x{:X} natives {} table 0x{:X}", s.name, s.nameHash, s.codeSize, s.nativeCount, s.nativeTable));
		}

		// Scan for NATIVE opcode calls (assumed legacy-VM encoding: op, (argc<<2|ret), idxHi, idxLo).
		// Naive linear byte scan: false positives are possible, results are hints only.
		void RunNativeCalls(const std::string& name, int opcode, int argc, int ret)
		{
			LoadedScript s;
			std::vector<std::uint8_t> code;
			if (!LoadScriptCode(name, s, code))
				return;

			std::vector<std::uintptr_t> table(s.nativeCount);
			g_Memory->ReadBuffer(s.nativeTable, table.data(), table.size() * sizeof(std::uintptr_t), false);
			auto* mod = GetGameModule();

			std::map<std::uint32_t, int> counts;
			const std::uint8_t sig = static_cast<std::uint8_t>((argc << 2) | (ret & 3));
			for (std::size_t i = 0; i + 4 <= code.size(); ++i)
			{
				if (code[i] == opcode && code[i + 1] == sig)
				{
					const std::uint32_t idx = (code[i + 2] << 8) | code[i + 3];
					if (idx < s.nativeCount)
						counts[idx]++;
				}
			}

			Out(std::format("{}: native call sites with argc={} ret={} (op 0x{:02X}): {} distinct natives", s.name, argc, ret, opcode, counts.size()));
			for (auto& [idx, c] : counts)
			{
				const auto h = table[idx];
				Out(std::format("  idx {:<5} calls {:<4} handler 0x{:X}{}", idx, c, h, (mod && h >= mod->Base()) ? std::format(" (RVA 0x{:X})", h - mod->Base()) : ""));
			}
		}

		// Show call sites of a given native index with preceding bytes (to see pushed constants)
		void RunCallSites(const std::string& name, int opcode, int nativeIdx)
		{
			LoadedScript s;
			std::vector<std::uint8_t> code;
			if (!LoadScriptCode(name, s, code))
				return;

			int found = 0;
			for (std::size_t i = 0; i + 4 <= code.size() && found < 30; ++i)
			{
				if (code[i] != opcode)
					continue;
				const std::uint32_t idx = (code[i + 2] << 8) | code[i + 3];
				if (idx != static_cast<std::uint32_t>(nativeIdx))
					continue;
				++found;
				std::string bytes;
				const std::size_t from = i >= 16 ? i - 16 : 0;
				for (std::size_t k = from; k < i + 4; ++k)
					bytes += std::format("{:02X} ", code[k]);
				Out(std::format("  PC 0x{:X} (argc/ret byte 0x{:02X}): {}", i, code[i + 1], bytes));
			}
			Out(std::format("{} call sites shown for idx {} in {}", found, nativeIdx, s.name));
		}

		// Dump handler bytes of a native by index, and its first call/jump targets are left for the user to examine.
		void RunHandlerDump(const std::string& name, int nativeIdx)
		{
			LoadedScript s;
			std::vector<std::uint8_t> code;
			if (!LoadScriptCode(name, s, code))
				return;
			if (nativeIdx < 0 || static_cast<std::uint32_t>(nativeIdx) >= s.nativeCount)
			{
				Out("native index out of range");
				return;
			}
			auto h = g_Memory->Read<std::uintptr_t>(s.nativeTable + nativeIdx * sizeof(std::uintptr_t));
			if (!h)
			{
				Out("read failed");
				return;
			}
			auto* mod = GetGameModule();
			Out(std::format("{} native idx {} handler 0x{:X}{}", s.name, nativeIdx, *h, (mod && *h >= mod->Base()) ? std::format(" (RVA 0x{:X})", *h - mod->Base()) : ""));
			Out(HexDump(*h, 96));
		}

		template<typename F>
		void RunAsync(F&& f)
		{
			if (g_Busy.exchange(true))
				return;
			std::thread([fn = std::forward<F>(f)]() mutable {
				try
				{
					fn();
				}
				catch (...)
				{
					Out("exception during diagnostics");
				}
				g_Busy = false;
			}).detach();
		}
	}

	void DrawNativeDiagnostics()
	{
		// Defaults: pattern widely used for rage::scrNativeRegistrationTable in legacy GTA V;
		// it is NOT verified for Enhanced - edit as needed.
		static char s_Pattern[256] = "76 61 49 8B 7A 40 48 8D 0D";
		static int s_RipOffset = 9;
		static int s_DumpLen = 128;
		static int s_MinCave = 64;
		static char s_Addr[32] = "";
		static int s_HexLen = 128;

		ImGui::SeparatorText("Native bridge diagnostics (read-only)");

		if (!g_Memory || !g_Memory->IsProcessAttached())
		{
			ImGui::TextDisabled("Not attached to process");
			return;
		}

		ImGui::BeginDisabled(g_Busy.load());

		ImGui::SetNextItemWidth(90);
		ImGui::InputInt("Min cave size", &s_MinCave, 0);
		ImGui::SameLine();
		if (ImGui::Button("Sections + code caves"))
			RunAsync([n = static_cast<std::size_t>(std::max(s_MinCave, 16))] { RunSectionsAndCaves(n); });

		ImGui::InputText("Pattern", s_Pattern, sizeof(s_Pattern));
		ImGui::SetNextItemWidth(90);
		ImGui::InputInt("RIP offset (-1 = none)", &s_RipOffset, 0);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90);
		ImGui::InputInt("Dump len", &s_DumpLen, 0);
		if (ImGui::Button("Scan pattern + dump"))
			RunAsync([p = std::string(s_Pattern), r = s_RipOffset, d = s_DumpLen] { RunPatternDump(p, r, d); });

		ImGui::SetNextItemWidth(160);
		ImGui::InputText("Address (hex)", s_Addr, sizeof(s_Addr));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90);
		ImGui::InputInt("Len", &s_HexLen, 0);
		ImGui::SameLine();
		if (ImGui::Button("Hex dump"))
		{
			const auto addr = static_cast<std::uintptr_t>(std::strtoull(s_Addr, nullptr, 16));
			RunAsync([addr, l = s_HexLen] { RunHexDump(addr, l); });
		}

		ImGui::SeparatorText("Script bytecode");
		static char s_Script[64] = "freemode";
		static int s_Opcode = 0x2C;
		static int s_Argc = 2;
		static int s_Ret = 0;
		static int s_Idx = 0;
		if (ImGui::Button("List scripts"))
			RunAsync([] { RunScriptList(); });
		ImGui::SetNextItemWidth(160);
		ImGui::InputText("Script", s_Script, sizeof(s_Script));
		ImGui::SetNextItemWidth(70);
		ImGui::InputInt("NATIVE opcode", &s_Opcode, 0);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(60);
		ImGui::InputInt("argc", &s_Argc, 0);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(60);
		ImGui::InputInt("ret", &s_Ret, 0);
		ImGui::SameLine();
		if (ImGui::Button("Find native calls"))
			RunAsync([n = std::string(s_Script), o = s_Opcode, a = s_Argc, r = s_Ret] { RunNativeCalls(n, o, a, r); });
		ImGui::SetNextItemWidth(70);
		ImGui::InputInt("native idx", &s_Idx, 0);
		ImGui::SameLine();
		if (ImGui::Button("Call sites"))
			RunAsync([n = std::string(s_Script), o = s_Opcode, i = s_Idx] { RunCallSites(n, o, i); });
		ImGui::SameLine();
		if (ImGui::Button("Handler dump"))
			RunAsync([n = std::string(s_Script), i = s_Idx] { RunHandlerDump(n, i); });

		ImGui::EndDisabled();

		if (g_Busy)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("working...");
		}

		if (ImGui::Button("Clear output"))
		{
			std::lock_guard lk(g_OutMutex);
			g_Output.clear();
		}
		ImGui::SameLine();
		if (ImGui::Button("Copy output"))
		{
			std::lock_guard lk(g_OutMutex);
			ImGui::SetClipboardText(g_Output.c_str());
		}

		{
			std::lock_guard lk(g_OutMutex);
			ImGui::InputTextMultiline("##nativediag", g_Output.data(), g_Output.size() + 1, ImVec2(-1, 260), ImGuiInputTextFlags_ReadOnly);
		}
	}
}
