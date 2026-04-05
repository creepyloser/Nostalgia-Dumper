#include "dumpers/dumper_shared.h"
#include "dumpers/engine_dumpers.h"
#include "memory.h"
#include "patterns.h"
#include <vector>

namespace Dumpers
{
    void RunUnityIl2Cpp(HANDLE h, std::map<std::string, u64>& sym, std::ostringstream& log)
    {
        log << "[*] Unity IL2CPP: GameAssembly + UnityPlayer exports (no Unreal globals).\n";
        Patterns::Scanner scanner(h);
        std::vector<ModuleInfo> allMods;
        Memory::EnumerateModules(h, allMods);

        const std::string gaName = Shared::FindModuleNameCi(h, "gameassembly.dll");
        const u64 ga = Shared::FindModuleBaseCi(h, "gameassembly.dll");
        if (ga && !gaName.empty())
        {
            sym["MOD_GameAssembly"] = ga;
            Shared::HarvestAllExports(h, ga, gaName, sym, log);
            if (u64 il2cpp_init = scanner.ScanModule(gaName,
                    "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC 20 48 8B F1 48 8B DA", 0))
                sym["IL2CPP_Init_Internal"] = il2cpp_init;
        }
        else
            log << "[-] gameassembly.dll not found — attach to the game process after load.\n";

        const std::string upName = Shared::FindModuleNameCi(h, "unityplayer.dll");
        const u64 up = Shared::FindModuleBaseCi(h, "unityplayer.dll");
        if (up && !upName.empty())
        {
            sym["MOD_UnityPlayer"] = up;
            Shared::HarvestAllExports(h, up, upName, sym, log);
        }
    }
}
