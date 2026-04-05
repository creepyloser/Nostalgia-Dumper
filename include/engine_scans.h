#pragma once

#include "types.h"
#include <map>
#include <string>

namespace EngineScans
{
    enum class Target : int
    {
        Unreal = 0,
        UnityIl2Cpp = 1,
        UnityMono = 2,
        Source1 = 3,
        Source2 = 4,
        Godot = 5,
        CryEngine = 6,
        Universal = 7,
        GameMaker = 8,
        UNIGINE = 9
    };

    const char* TargetDisplayName(Target t);

    Result Run(Target t, u32 processId, DiscoveredOffsets& outOffsets, std::map<std::string, u64>& outSymbols,
               std::string& outLog, bool unrealExhaustiveSecondary = false);
}
