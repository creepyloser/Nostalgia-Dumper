#pragma once

#include "memory.h"
#include "patterns.h"
#include "pe_remote.h"
#include "types.h"
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Dumpers::Shared
{
    std::string ModuleNameLower(std::string_view raw);
    std::string SanitizeKeySuffix(std::string_view raw);
    void AppendModuleBases(HANDLE h, std::map<std::string, u64>& sym);
    u64 FindModuleBaseCi(HANDLE h, const char* needleLower);
    std::string FindModuleNameCi(HANDLE h, const char* needleLower);
    bool EndsWithDllCi(std::string_view name);

    u64 ScanRipGlobal(Patterns::Scanner& scanner, HANDLE h, const std::vector<ModuleInfo>& mods, const char* pattern,
        int ripDispOffset = 3);

    void HarvestAllExports(HANDLE h, u64 base, const std::string& modName, std::map<std::string, u64>& sym,
        std::ostringstream& log);

    /** Records tier0.dll CreateInterface export if present (Source / Source 2). */
    void HarvestCreateInterface(HANDLE h, std::map<std::string, u64>& sym, std::ostringstream& log);

    /** Harvest exports from every non-trivial .dll not already in harvestedLower (case-insensitive name). */
    void HarvestAllUnseenDlls(HANDLE h, std::map<std::string, u64>& sym, std::ostringstream& log,
        std::unordered_set<std::string>& harvestedLower, size_t minImageBytes = 32 * 1024);
}
