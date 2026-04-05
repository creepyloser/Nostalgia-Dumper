#include "engine_scans.h"
#include "dumper.h"
#include "memory.h"
#include "patterns.h"
#include "pe_remote.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string_view>

namespace
{
    std::string ModuleNameLower(std::string_view raw)
    {
        std::string l(raw);
        std::transform(l.begin(), l.end(), l.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return l;
    }

    std::string SanitizeKeySuffix(std::string_view raw)
    {
        std::string s;
        s.reserve(raw.size());
        for (char c : raw)
        {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
                s += c;
            else
                s += '_';
        }
        return s;
    }

    void AppendModuleBases(HANDLE h, std::map<std::string, u64>& sym)
    {
        std::vector<ModuleInfo> mods;
        if (Memory::EnumerateModules(h, mods) <= 0)
            return;
        for (const auto& m : mods)
        {
            const std::string key = "MOD_" + SanitizeKeySuffix(m.name);
            sym[key] = m.base;
        }
    }

    u64 FindModuleBaseCi(HANDLE h, const char* needleLower)
    {
        std::vector<ModuleInfo> mods;
        if (Memory::EnumerateModules(h, mods) <= 0)
            return 0;
        for (const auto& m : mods)
        {
            if (ModuleNameLower(m.name) == needleLower)
                return m.base;
        }
        return 0;
    }

    void HarvestAllExports(HANDLE h, u64 base, const std::string& modName, std::map<std::string, u64>& sym, std::ostringstream& log)
    {
        std::vector<PeRemote::ExportEntry> ex;
        if (!PeRemote::EnumerateExports(h, base, ex)) return;
        
        std::string prefix = "EXP_" + SanitizeKeySuffix(modName) + "_";
        for (const auto& e : ex)
        {
            if (!e.name.empty())
                sym[prefix + e.name] = e.va;
        }
        log << "[+] Harvested " << ex.size() << " exports from " << modName << "\n";
    }
}

namespace EngineScans
{
    const char* TargetDisplayName(Target t)
    {
        switch (t)
        {
            case Target::Unreal: return "Unreal Engine";
            case Target::UnityIl2Cpp: return "Unity (IL2CPP)";
            case Target::UnityMono: return "Unity (Mono)";
            case Target::Source1: return "Source";
            case Target::Source2: return "Source 2";
            case Target::Godot: return "Godot Engine";
            case Target::CryEngine: return "CryEngine";
            case Target::Universal: return "Universal (All Exports)";
            case Target::GameMaker: return "GameMaker Studio";
            case Target::UNIGINE: return "UNIGINE";
            default: return "Unknown";
        }
    }

    Result Run(Target t, u32 processId, DiscoveredOffsets& outOffsets, std::map<std::string, u64>& outSymbols,
               std::string& outLog, bool unrealExhaustiveSecondary)
    {
        outLog.clear();
        outSymbols.clear();
        outOffsets = DiscoveredOffsets();
        std::ostringstream log;

        if (t == Target::Unreal)
        {
            return Dumper::ScanProcess(processId, outOffsets, outLog, unrealExhaustiveSecondary);
        }

        HANDLE h = Memory::OpenProcessHandle(processId);
        if (!h || h == INVALID_HANDLE_VALUE)
        {
            outLog = "Cannot open process.";
            return { ScanResult::CannotOpenProcess, outLog };
        }

        Patterns::Scanner scanner(h);

        if (t == Target::Universal)
        {
            std::vector<ModuleInfo> mods;
            Memory::EnumerateModules(h, mods);
            for (const auto& m : mods)
                HarvestAllExports(h, m.base, m.name, outSymbols, log);
        }
        else if (t == Target::GameMaker)
        {
            log << "[*] Scanning for GameMaker Studio globals...\n";
            // YYObjectArray signature
            u64 addr = scanner.ScanModule("", "48 8B 05 ? ? ? ? 48 8B 0C C8 48 8B 01", 3);
            if (addr) {
                i32 rel = Memory::ReadDword(h, addr);
                outSymbols["YYObjectArray"] = Memory::ResolveRelativeAddress(addr, rel);
                log << "[+] Found YYObjectArray at 0x" << std::hex << outSymbols["YYObjectArray"] << "\n";
            }
        }
        else if (t == Target::UNIGINE)
        {
            log << "[*] Scanning for UNIGINE structures...\n";
            // Unigine::World signature
            u64 addr = scanner.ScanModule("", "48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ? 48 85 C9 74 05", 3);
            if (addr) {
                i32 rel = Memory::ReadDword(h, addr);
                outSymbols["Unigine_World"] = Memory::ResolveRelativeAddress(addr, rel);
                log << "[+] Found UNIGINE World at 0x" << std::hex << outSymbols["Unigine_World"] << "\n";
            }
        }
        else if (t == Target::Godot)
        {
            log << "[*] Scanning for Godot Engine structures...\n";
            std::vector<ModuleInfo> mods;
            Memory::EnumerateModules(h, mods);
            for (const auto& m : mods) {
                if (ModuleNameLower(m.name).find("godot") != std::string::npos) {
                    outSymbols["MOD_GodotCore"] = m.base;
                    HarvestAllExports(h, m.base, m.name, outSymbols, log);
                }
            }
        }
        else if (t == Target::CryEngine)
        {
            log << "[*] Scanning for CryEngine structures...\n";
            u64 system = FindModuleBaseCi(h, "crysystem.dll");
            if (system) {
                outSymbols["MOD_CrySystem"] = system;
                HarvestAllExports(h, system, "CrySystem.dll", outSymbols, log);
            }
        }
        else if (t == Target::UnityIl2Cpp)
        {
            u64 ga = FindModuleBaseCi(h, "gameassembly.dll");
            if (ga) {
                outSymbols["MOD_GameAssembly"] = ga;
                HarvestAllExports(h, ga, "GameAssembly.dll", outSymbols, log);
                
                // Additional IL2CPP specific discovery
                u64 il2cpp_init = scanner.ScanModule("GameAssembly.dll", "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B F1 48 8B DA");
                if (il2cpp_init) outSymbols["IL2CPP_Init_Internal"] = il2cpp_init;
            }
        }
        else if (t == Target::UnityMono)
        {
            u64 mono = FindModuleBaseCi(h, "mono-2.0-bdwgc.dll");
            if (!mono) mono = FindModuleBaseCi(h, "mono.dll");
            if (mono) {
                outSymbols["MOD_Mono"] = mono;
                HarvestAllExports(h, mono, "Mono.dll", outSymbols, log);
                
                // Mono JIT info
                u64 jit_info = scanner.ScanModule(ModuleNameLower("mono-2.0-bdwgc.dll"), "48 83 EC 28 48 8B 05 ? ? ? ? 48 85 C0 74 02");
                if (jit_info) outSymbols["MONO_JitInfo_Internal"] = jit_info;
            }
        }

        AppendModuleBases(h, outSymbols);
        Memory::CloseProcessHandle(h);
        outLog = log.str();
        return { ScanResult::Success, "OK" };
    }
}
