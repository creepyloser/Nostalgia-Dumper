#include "dumpers/dumper_shared.h"
#include "dumpers/engine_dumpers.h"
#include "memory.h"
#include "patterns.h"
#include <vector>

namespace Dumpers
{
    void RunUnigine(HANDLE h, std::map<std::string, u64>& sym, std::ostringstream& log)
    {
        log << "[*] UNIGINE: pattern scan across all modules.\n";
        Patterns::Scanner scanner(h);
        std::vector<ModuleInfo> allMods;
        Memory::EnumerateModules(h, allMods);
        if (u64 w = Shared::ScanRipGlobal(scanner, h, allMods, "48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ? 48 85 C9 74 05", 3))
        {
            sym["Unigine_World"] = w;
            log << "[+] Unigine_World -> 0x" << std::hex << w << std::dec << "\n";
        }
        if (u64 eng = Shared::ScanRipGlobal(scanner, h, allMods, "48 8B 05 ? ? ? ? 48 8B 80 ? ? ? ? 48 85 C0 74", 3))
        {
            sym["Unigine_Engine"] = eng;
            log << "[+] Unigine_Engine -> 0x" << std::hex << eng << std::dec << "\n";
        }
    }
}
