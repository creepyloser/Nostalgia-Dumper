#include "dumpers/dumper_shared.h"
#include "dumpers/engine_dumpers.h"
#include "memory.h"
#include <unordered_set>

namespace Dumpers
{
    namespace
    {
        void HarvestKnown(HANDLE h, const char* const* dlls, size_t n, std::map<std::string, u64>& sym,
            std::ostringstream& log, std::unordered_set<std::string>& harvestedLower)
        {
            for (size_t i = 0; i < n; ++i)
            {
                const std::string real = Shared::FindModuleNameCi(h, dlls[i]);
                if (real.empty())
                    continue;
                const u64 b = Shared::FindModuleBaseCi(h, dlls[i]);
                if (!b)
                    continue;
                Shared::HarvestAllExports(h, b, real, sym, log);
                harvestedLower.insert(Shared::ModuleNameLower(real));
            }
        }
    }

    void RunSource1(HANDLE h, std::map<std::string, u64>& sym, std::ostringstream& log)
    {
        log << "[*] Source 1: tier0 CreateInterface + priority DLLs + remaining game DLL exports.\n";
        std::unordered_set<std::string> seen;
        static const char* kPriority[] = { "engine.dll", "client.dll", "tier0.dll", "server.dll", "vstdlib.dll",
            "filesystem_stdio.dll", "datacache.dll", "materialsystem.dll", "vphysics.dll", "studiorender.dll",
            "shaderapidx9.dll", "inputsystem.dll", "localize.dll", "vgui2.dll", "matchmaking.dll",
            "soundemittersystem.dll", "sceneprocessor.dll", "stdshader_dx9.dll", "binkw64.dll" };
        HarvestKnown(h, kPriority, sizeof(kPriority) / sizeof(kPriority[0]), sym, log, seen);
        Shared::HarvestCreateInterface(h, sym, log);
        Shared::HarvestAllUnseenDlls(h, sym, log, seen);
    }

    void RunSource2(HANDLE h, std::map<std::string, u64>& sym, std::ostringstream& log)
    {
        log << "[*] Source 2: tier0 CreateInterface + priority DLLs + remaining game DLL exports.\n";
        std::unordered_set<std::string> seen;
        static const char* kPriority[] = { "engine2.dll", "client.dll", "tier0.dll", "vstdlib.dll",
            "filesystem_stdio.dll", "materialsystem2.dll", "panorama.dll", "networksystem.dll", "rendersystemdx11.dll",
            "resourcesystem.dll", "schemasystem.dll", "particles.dll", "vphysics2.dll", "studiorender.dll",
            "animationsystem.dll", "scenesystem.dll", "worldrenderer.dll", "steam_audio.dll", "localize.dll",
            "host.dll", "navsystem.dll", "v8.dll", "v8system.dll" };
        HarvestKnown(h, kPriority, sizeof(kPriority) / sizeof(kPriority[0]), sym, log, seen);
        Shared::HarvestCreateInterface(h, sym, log);
        Shared::HarvestAllUnseenDlls(h, sym, log, seen);
    }
}
