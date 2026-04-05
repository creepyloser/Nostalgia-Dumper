#include "dumpers/dumper_shared.h"
#include "dumpers/engine_dumpers.h"
#include "memory.h"
#include <vector>

namespace Dumpers
{
    void RunGodot(HANDLE h, std::map<std::string, u64>& sym, std::ostringstream& log)
    {
        log << "[*] Godot: modules with \"godot\" in the name + exports.\n";
        std::vector<ModuleInfo> mods;
        Memory::EnumerateModules(h, mods);
        for (const auto& m : mods)
        {
            if (Shared::ModuleNameLower(m.name).find("godot") != std::string::npos)
            {
                sym["MOD_GodotCore"] = m.base;
                Shared::HarvestAllExports(h, m.base, m.name, sym, log);
            }
        }
    }
}
