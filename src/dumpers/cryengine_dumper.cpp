#include "dumpers/dumper_shared.h"
#include "dumpers/engine_dumpers.h"

namespace Dumpers
{
    void RunCryEngine(HANDLE h, std::map<std::string, u64>& sym, std::ostringstream& log)
    {
        log << "[*] CryEngine: CrySystem.dll export harvest.\n";
        const u64 system = Shared::FindModuleBaseCi(h, "crysystem.dll");
        if (system)
        {
            sym["MOD_CrySystem"] = system;
            Shared::HarvestAllExports(h, system, "CrySystem.dll", sym, log);
        }
        else
            log << "[-] crysystem.dll not found.\n";
    }
}
