#include "dumpers/dumper_shared.h"
#include "dumpers/engine_dumpers.h"
#include "memory.h"
#include "patterns.h"

namespace Dumpers
{
    void RunUnityMono(HANDLE h, std::map<std::string, u64>& sym, std::ostringstream& log)
    {
        log << "[*] Unity Mono: mono runtime exports + JIT/domain hints.\n";
        Patterns::Scanner scanner(h);
        static const char* kMonoDlls[] = { "mono-2.0-bdwgc.dll", "mono.dll", "mono-2.0-sgen.dll" };
        for (const char* dn : kMonoDlls)
        {
            const std::string real = Shared::FindModuleNameCi(h, dn);
            const u64 mono = Shared::FindModuleBaseCi(h, dn);
            if (!mono || real.empty())
                continue;
            sym["MOD_Mono"] = mono;
            Shared::HarvestAllExports(h, mono, real, sym, log);
            if (u64 jit = scanner.ScanModule(real, "48 83 EC 28 48 8B 05 ? ? ? ? 48 85 C0 74 02", 0))
                sym["MONO_JitInfo_Internal"] = jit;
            if (u64 domain = scanner.ScanModule(real, "48 8B 05 ? ? ? ? 48 85 C0 74 ? 48 8B 08", 3))
            {
                const i32 rel = Memory::ReadDword(h, domain);
                sym["MONO_Domain_Candidate"] = Memory::ResolveRelativeAddress(domain, rel, 4);
            }
            break;
        }
    }
}
