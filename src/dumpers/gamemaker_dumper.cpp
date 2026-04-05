#include "dumpers/dumper_shared.h"
#include "dumpers/engine_dumpers.h"
#include "memory.h"
#include "patterns.h"
#include <vector>

namespace Dumpers
{
    void RunGameMaker(HANDLE h, std::map<std::string, u64>& sym, std::ostringstream& log)
    {
        log << "[*] GameMaker: pattern scan across all modules.\n";
        Patterns::Scanner scanner(h);
        std::vector<ModuleInfo> allMods;
        Memory::EnumerateModules(h, allMods);
        if (u64 yy = Shared::ScanRipGlobal(scanner, h, allMods, "48 8B 05 ? ? ? ? 48 8B 0C C8 48 8B 01", 3))
        {
            sym["YYObjectArray"] = yy;
            log << "[+] YYObjectArray -> 0x" << std::hex << yy << std::dec << "\n";
        }
        if (u64 gvar = Shared::ScanRipGlobal(scanner, h, allMods,
                "48 8B 05 ? ? ? ? 48 8B 0C C8 48 8B 01 48 85 C9 74 05", 3))
        {
            sym["GlobalVariableMap"] = gvar;
            log << "[+] GlobalVariableMap -> 0x" << std::hex << gvar << std::dec << "\n";
        }
    }
}
