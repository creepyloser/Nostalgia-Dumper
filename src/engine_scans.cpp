#include "dumpers/dumper_shared.h"
#include "dumpers/engine_dumpers.h"
#include "engine_scans.h"
#include "dumper.h"
#include "memory.h"
#include <sstream>
#include <string>

namespace EngineScans
{
    const char* TargetDisplayName(Target t)
    {
        switch (t)
        {
            case Target::Unreal: return "Unreal Engine";
            case Target::UnityIl2Cpp: return "Unity (IL2CPP)";
            case Target::UnityMono: return "Unity (Mono)";
            case Target::Source1: return "Source";
            case Target::Source2: return "Source 2";
            case Target::Godot: return "Godot Engine";
            case Target::CryEngine: return "CryEngine";
            case Target::Universal: return "Universal (All Exports)";
            case Target::GameMaker: return "GameMaker Studio";
            case Target::UNIGINE: return "UNIGINE";
            default: return "Unknown";
        }
    }

    Result Run(Target t, u32 processId, DiscoveredOffsets& outOffsets, std::map<std::string, u64>& outSymbols,
        std::string& outLog, bool unrealExhaustiveSecondary)
    {
        outLog.clear();
        outSymbols.clear();
        outOffsets = DiscoveredOffsets();

        if (t == Target::Unreal)
            return Dumper::ScanProcess(processId, outOffsets, outLog, unrealExhaustiveSecondary);

        HANDLE h = Memory::OpenProcessHandle(processId);
        if (!h || h == INVALID_HANDLE_VALUE)
        {
            outLog = "Cannot open process.";
            return { ScanResult::CannotOpenProcess, outLog };
        }

        std::ostringstream log;
        switch (t)
        {
            case Target::Universal: Dumpers::RunUniversal(h, outSymbols, log); break;
            case Target::GameMaker: Dumpers::RunGameMaker(h, outSymbols, log); break;
            case Target::UNIGINE: Dumpers::RunUnigine(h, outSymbols, log); break;
            case Target::Godot: Dumpers::RunGodot(h, outSymbols, log); break;
            case Target::CryEngine: Dumpers::RunCryEngine(h, outSymbols, log); break;
            case Target::Source1: Dumpers::RunSource1(h, outSymbols, log); break;
            case Target::Source2: Dumpers::RunSource2(h, outSymbols, log); break;
            case Target::UnityIl2Cpp: Dumpers::RunUnityIl2Cpp(h, outSymbols, log); break;
            case Target::UnityMono: Dumpers::RunUnityMono(h, outSymbols, log); break;
            default:
                Memory::CloseProcessHandle(h);
                outLog = "Unsupported engine target.";
                return { ScanResult::NoOffsetsFound, outLog };
        }

        Dumpers::Shared::AppendModuleBases(h, outSymbols);
        Memory::CloseProcessHandle(h);
        outLog = log.str();
        return { ScanResult::Success, "OK" };
    }
}
