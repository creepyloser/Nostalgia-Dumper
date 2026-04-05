#include "dumpers/dumper_shared.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>

namespace Dumpers::Shared
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
            sym["MOD_" + SanitizeKeySuffix(m.name)] = m.base;
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

    std::string FindModuleNameCi(HANDLE h, const char* needleLower)
    {
        std::vector<ModuleInfo> mods;
        if (Memory::EnumerateModules(h, mods) <= 0)
            return {};
        for (const auto& m : mods)
        {
            if (ModuleNameLower(m.name) == needleLower)
                return m.name;
        }
        return {};
    }

    bool EndsWithDllCi(std::string_view name)
    {
        if (name.size() < 4)
            return false;
        return ModuleNameLower(name.substr(name.size() - 4)) == ".dll";
    }

    u64 ScanRipGlobal(Patterns::Scanner& scanner, HANDLE h, const std::vector<ModuleInfo>& mods, const char* pattern,
        int ripDispOffset)
    {
        for (const auto& m : mods)
        {
            const u64 hit = scanner.ScanModule(m.name, pattern, ripDispOffset);
            if (!hit)
                continue;
            const i32 rel = Memory::ReadDword(h, hit);
            return Memory::ResolveRelativeAddress(hit, rel, 4);
        }
        return 0;
    }

    void HarvestAllExports(HANDLE h, u64 base, const std::string& modName, std::map<std::string, u64>& sym,
        std::ostringstream& log)
    {
        std::vector<PeRemote::ExportEntry> ex;
        if (!PeRemote::EnumerateExports(h, base, ex))
            return;

        const std::string prefix = "EXP_" + SanitizeKeySuffix(modName) + "_";
        for (const auto& e : ex)
        {
            if (!e.name.empty())
                sym[prefix + e.name] = e.va;
        }
        log << "[+] Harvested " << ex.size() << " exports from " << modName << "\n";
    }

    void HarvestCreateInterface(HANDLE h, std::map<std::string, u64>& sym, std::ostringstream& log)
    {
        const u64 base = FindModuleBaseCi(h, "tier0.dll");
        if (!base)
            return;
        std::vector<PeRemote::ExportEntry> ex;
        if (!PeRemote::EnumerateExports(h, base, ex))
            return;
        for (const auto& e : ex)
        {
            if (e.name == "CreateInterface")
            {
                sym["SRC_CreateInterface"] = e.va;
                log << "[+] CreateInterface (tier0.dll) -> 0x" << std::hex << e.va << std::dec << "\n";
                return;
            }
        }
    }

    void HarvestAllUnseenDlls(HANDLE h, std::map<std::string, u64>& sym, std::ostringstream& log,
        std::unordered_set<std::string>& harvestedLower, size_t minImageBytes)
    {
        std::vector<ModuleInfo> mods;
        if (Memory::EnumerateModules(h, mods) <= 0)
            return;

        int extra = 0;
        for (const auto& m : mods)
        {
            const std::string low = ModuleNameLower(m.name);
            if (harvestedLower.count(low))
                continue;
            if (m.imageSize < minImageBytes)
                continue;
            if (!EndsWithDllCi(m.name))
                continue;
            HarvestAllExports(h, m.base, m.name, sym, log);
            harvestedLower.insert(low);
            ++extra;
        }
        log << "[*] Extra DLL export pass: " << std::dec << extra << " additional modules.\n";
    }
}
