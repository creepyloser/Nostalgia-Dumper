#include "dumpers/dumper_shared.h"
#include "dumpers/engine_dumpers.h"
#include "memory.h"
#include <vector>

namespace Dumpers
{
    void RunUniversal(HANDLE h, std::map<std::string, u64>& sym, std::ostringstream& log)
    {
        log << "[*] Universal: export harvest for every loaded module.\n";
        std::vector<ModuleInfo> mods;
        Memory::EnumerateModules(h, mods);
        for (const auto& m : mods)
            Shared::HarvestAllExports(h, m.base, m.name, sym, log);
    }
}
