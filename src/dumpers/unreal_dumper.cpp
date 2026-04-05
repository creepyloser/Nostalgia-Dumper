#include "dumper.h"
#include "memory.h"
#include "patterns.h"
#include "version.h"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace Dumper
{
    namespace
    {
        void ApplyDefaultMemberLayouts(DiscoveredOffsets& o)
        {
            o.layoutMemberOffsets.clear();
            static const std::pair<const char*, u64> kMembers[] = {
                {"UObject_ObjectFlags", 0x8},
                {"UObject_InternalIndex", 0xC},
                {"UObject_ClassPrivate", 0x10},
                {"UObject_NamePrivate", 0x18},
                {"UObject_OuterPrivate", 0x20},
                {"UStruct_SuperStruct", 0x40},
                {"UStruct_Children", 0x48},
                {"UStruct_ChildProperties", 0x50},
                {"UStruct_PropertiesSize", 0x78},
                {"UStruct_MinAlignment", 0x7C},
                {"UField_Next", 0x28},
                {"UEnum_Names", 0x40},
                {"UScriptStruct_StructSize", 0x58},
                {"UFunction_FunctionFlags", 0xB8},
                {"UFunction_NumParms", 0xC0},
                {"UFunction_ParmsSize", 0xC2},
                {"UFunction_ReturnValueOffset", 0xC4},
                {"UFunction_RPCId", 0xCA},
                {"UFunction_RPCResponseId", 0xCC},
                {"UFunction_FirstPropertyToInit", 0x70},
                {"UProperty_ElementSize", 0x34},
                {"UProperty_ArrayDim", 0x30},
                {"UProperty_PropertyFlags", 0x38},
                {"UProperty_Offset_Internal", 0x44},
                {"UBoolProperty_FieldMask", 0x78},
                {"UClass_ClassDefaultObject", 0x110},
                {"UClass_ImplementedInterfaces", 0x1D0},
                {"AActor_RootComponent", 0x1B0},
                {"UWorld_PersistentLevel", 0x30},
                {"UWorld_OwningGameInstance", 0x1D8},
                {"UGameInstance_LocalPlayers", 0x38},
                {"UPlayer_PlayerController", 0x30},
                {"APlayerController_AcknowledgedPawn", 0x338},
                {"APlayerController_PlayerState", 0x2D0},
                {"APlayerController_PlayerCameraManager", 0x340},
                {"APlayerController_ControlRotation", 0x2F8},
                {"ULocalPlayer_ViewportClient", 0x78},
                {"APawn_PlayerState", 0x2B0},
                {"APawn_Controller", 0x2A8},
                {"ACharacter_Mesh", 0x318},
                {"ACharacter_CharacterMovement", 0x320},
                {"USceneComponent_RelativeLocation", 0x128},
                {"USceneComponent_RelativeRotation", 0x140},
                {"USceneComponent_RelativeScale3D", 0x158},
                {"USceneComponent_AttachParent", 0x1A0},
                {"USceneComponent_AttachChildren", 0x1A8},
                {"UPrimitiveComponent_BoundsScale", 0x24C},
                {"USkinnedMeshComponent_SkeletalMesh", 0x2B8},
                {"USkeletalMeshComponent_AnimInstance", 0x660},
                {"USkeletalMeshComponent_MasterPoseComponent", 0x6B8},
                {"UStaticMeshComponent_StaticMesh", 0x558},
                {"ULevel_Actors", 0x98},
                {"ULevel_WorldSettings", 0xB8},
                {"ULevel_Model", 0xC0},
                {"ULevel_URL", 0x68},
                {"UGameViewportClient_World", 0x78},
                {"UGameViewportClient_GameInstance", 0x80},
                {"FMinimalViewInfo_Location", 0x0},
                {"FMinimalViewInfo_Rotation", 0x18},
                {"FMinimalViewInfo_FOV", 0x30},
                {"UWorld_Levels", 0x170},
                {"UWorld_CurrentLevel", 0x30},
                {"UWorld_LineBatcher", 0x718},
                {"UWorld_PersistentLineBatcher", 0x720},
                {"UWorld_ForegroundLineBatcher", 0x728},
                {"UWorld_NavigationSystem", 0x730},
                {"UWorld_AuthorityGameMode", 0x1A8},
                {"AGameModeBase_GameState", 0x2D0},
                {"AGameStateBase_PlayerArray", 0x2A8},
                {"UObject_CreateExport", 0x0},
                {"UDataTable_RowMap", 0x30},
                {"UAssetManager_ObjectReferenceList", 0x0},
            };
            for (const auto& e : kMembers)
                o.layoutMemberOffsets[e.first] = e.second;
        }

        bool IsPathChar(char c)
        {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.'
                || c == '/' || c == ':' || c == '-' || c == ' ';
        }

        void CollectScriptPathHints(HANDLE h, const std::vector<std::string>& modules, DiscoveredOffsets& out,
            std::ostringstream& log, size_t maxHints = 1500, size_t maxBytesPerModule = 28 * 1024 * 1024)
        {
            static const char* needles[] = { "/Script/", "/Game/" };
            std::unordered_set<std::string> seen;
            seen.reserve(maxHints * 2);

            for (const auto& modName : modules)
            {
                if (out.scriptPathHints.size() >= maxHints)
                    break;
                const u64 base = Memory::GetModuleBase(h, modName);
                const size_t img = Memory::GetModuleSize(h, modName);
                if (!base || !img)
                    continue;

                const size_t toRead = (std::min)(img, maxBytesPerModule);
                std::vector<u8> buf(toRead);
                const size_t n = Memory::ReadMemory(h, base, buf.data(), toRead);
                if (n < 12)
                    continue;

                for (const char* nd : needles)
                {
                    const size_t ndLen = std::strlen(nd);
                    for (size_t i = 0; i + ndLen < n && out.scriptPathHints.size() < maxHints; ++i)
                    {
                        if (std::memcmp(buf.data() + i, nd, ndLen) != 0)
                            continue;
                        size_t j = i;
                        while (j < n && (j - i) < 192 && IsPathChar(static_cast<char>(buf[j])))
                            ++j;
                        if (j - i < ndLen + 3)
                            continue;
                        std::string s(reinterpret_cast<const char*>(buf.data() + i), j - i);
                        if (seen.insert(s).second)
                            out.scriptPathHints.push_back(std::move(s));
                    }
                }
            }

            log << "[*] Reflection string hints (/Script/, /Game/): " << std::dec << out.scriptPathHints.size() << "\n";
        }

        void TrySampleUObjects(HANDLE h, u64 guobjArrayPtr, std::vector<std::string>& out, std::ostringstream& log,
            size_t maxSamples = 48)
        {
            if (!guobjArrayPtr || !Memory::IsValidAddress(guobjArrayPtr))
                return;

            auto tryChunked = [&]() -> bool {
                const u64 chunkTable = Memory::ReadQword(h, guobjArrayPtr);
                const i32 numEl = static_cast<i32>(Memory::ReadDword(h, guobjArrayPtr + 0x0C));
                const i32 numCh = static_cast<i32>(Memory::ReadDword(h, guobjArrayPtr + 0x14));
                if (!Memory::IsValidAddress(chunkTable) || numEl <= 0 || numEl > 50'000'000 || numCh <= 0
                    || numCh > 100'000)
                    return false;

                constexpr i32 kChunkElems = 65536;
                constexpr u64 kStride = 24;
                const i32 toScan = static_cast<i32>((std::min)(static_cast<i32>(maxSamples), numEl));
                for (i32 idx = 0; idx < toScan; ++idx)
                {
                    const i32 chunk = idx / kChunkElems;
                    const i32 within = idx % kChunkElems;
                    const u64 chunkPtr = Memory::ReadQword(h, chunkTable + static_cast<u64>(chunk) * 8u);
                    if (!chunkPtr || !Memory::IsValidAddress(chunkPtr))
                        continue;
                    const u64 item = chunkPtr + static_cast<u64>(within) * kStride;
                    const u64 uo = Memory::ReadQword(h, item);
                    if (!uo || !Memory::IsValidAddress(uo))
                        continue;
                    u64 cls = Memory::ReadQword(h, uo + 0x10);
                    if (!cls || !Memory::IsValidAddress(cls))
                        cls = Memory::ReadQword(h, uo + 0x8);
                    std::ostringstream line;
                    line << "i=" << idx << " obj=0x" << std::hex << std::uppercase << uo << " class=0x" << cls << std::dec;
                    out.push_back(line.str());
                    if (out.size() >= maxSamples)
                        return true;
                }
                return !out.empty();
            };

            auto tryLinear = [&]() -> bool {
                const u64 objTable = Memory::ReadQword(h, guobjArrayPtr + 0x10);
                const i32 numEl = static_cast<i32>(Memory::ReadDword(h, guobjArrayPtr + 0x1C));
                if (!Memory::IsValidAddress(objTable) || numEl <= 0 || numEl > 50'000'000)
                    return false;

                constexpr u64 kStride = 24;
                const i32 toScan = static_cast<i32>((std::min)(static_cast<i32>(maxSamples), numEl));
                for (i32 idx = 0; idx < toScan; ++idx)
                {
                    const u64 item = objTable + static_cast<u64>(idx) * kStride;
                    const u64 uo = Memory::ReadQword(h, item);
                    if (!uo || !Memory::IsValidAddress(uo))
                        continue;
                    u64 cls = Memory::ReadQword(h, uo + 0x10);
                    if (!cls || !Memory::IsValidAddress(cls))
                        cls = Memory::ReadQword(h, uo + 0x8);
                    std::ostringstream line;
                    line << "i=" << idx << " obj=0x" << std::hex << std::uppercase << uo << " class=0x" << cls << std::dec;
                    out.push_back(line.str());
                    if (out.size() >= maxSamples)
                        return true;
                }
                return !out.empty();
            };

            if (!tryChunked() && !tryLinear())
                log << "[*] UObject samples: layout not recognized (verify GUObjectArray layout for this build).\n";
            else
                log << "[*] UObject samples: " << std::dec << out.size() << " entries (raw pointers; FName decode separate).\n";
        }
    }

    Result ScanProcess(u32 processId, DiscoveredOffsets& outOffsets, std::string& outLog, bool exhaustiveUeSecondary)
    {
        std::ostringstream log;

        HANDLE processHandle = Memory::OpenProcessHandle(processId);
        if (!processHandle || processHandle == INVALID_HANDLE_VALUE)
        {
            log << "ERROR: Failed to open process (PID: " << processId << ")\n";
            outLog = log.str();
            return { ScanResult::CannotOpenProcess, "Cannot open process handle" };
        }

        log << "[*] Process opened successfully (PID: " << std::dec << processId << ")\n";

        std::vector<std::string> scanModules = Memory::GetUEScanModuleOrder(processHandle);
        if (scanModules.empty())
        {
            log << "[!] ERROR: No modules found in target process\n";
            Memory::CloseProcessHandle(processHandle);
            outLog = log.str();
            return { ScanResult::ModuleNotFound, "No modules in process" };
        }

        log << "[+] Scanning " << scanModules.size() << " module(s) (priority: engine.dll / unreal.dll / UE modules + size order)\n";
        const size_t kMaxLogMods = 24;
        for (size_t i = 0; i < scanModules.size() && i < kMaxLogMods; ++i)
            log << "    [" << i << "] " << scanModules[i] << "\n";
        if (scanModules.size() > kMaxLogMods)
            log << "    ... +" << (scanModules.size() - kMaxLogMods) << " more\n";
        if (exhaustiveUeSecondary)
            log << "\n[*] Deep UE scan: extra signatures + heuristics (slower).\n";
        else
            log << "\n[*] UE scan: GObjects, FNamePool/GNames, GWorld, GEngine + ProcessEvent candidates.\n";

        outOffsets = DiscoveredOffsets();
        Patterns::DiscoverAllOffsets(processHandle, scanModules, outOffsets, exhaustiveUeSecondary);
        ApplyDefaultMemberLayouts(outOffsets);

        auto logGlobal = [&](const char* name, const OffsetInfo& info) {
            if (info.address)
                log << "    [+] " << name << ": 0x" << std::hex << std::uppercase << info.address << std::dec << " (via "
                    << info.signature << ")\n";
            else
                log << "    [-] " << name << " primary not resolved\n";
        };
        log << "\n[*] Core globals (primary pick; extras in custom list):\n";
        logGlobal("GObjects", outOffsets.GObjects);
        logGlobal("GNames", outOffsets.GNames);
        logGlobal("GWorld", outOffsets.GWorld);
        logGlobal("GEngine", outOffsets.GEngine);
        if (outOffsets.GObjects.address)
            outOffsets.GUObjectArray = outOffsets.GObjects;
        if (outOffsets.GNames.address)
            outOffsets.FNamePool = outOffsets.GNames;

        log << "\n[*] Pattern hits: " << std::dec << outOffsets.customOffsets.size()
            << " candidate address(es) in customOffsets\n";
        log << "[*] Layout macros: " << outOffsets.layoutMemberOffsets.size()
            << " estimated UObject/class member offsets (verify per build)\n";

        CollectScriptPathHints(processHandle, scanModules, outOffsets, log);
        if (outOffsets.GObjects.address)
            TrySampleUObjects(processHandle, outOffsets.GObjects.address, outOffsets.uobjectSamples, log);

        Memory::CloseProcessHandle(processHandle);

        log << "\n[*] Scan completed.\n";
        log << "\n[*] Results summary:\n";
        log << "    - GObjects:  " << (outOffsets.GObjects.address ? "FOUND" : "NOT FOUND") << "\n";
        log << "    - GNames:    " << (outOffsets.GNames.address ? "FOUND" : "NOT FOUND") << "\n";
        log << "    - GWorld:    " << (outOffsets.GWorld.address ? "FOUND" : "NOT FOUND") << "\n";
        log << "    - GEngine:   " << (outOffsets.GEngine.address ? "FOUND" : "NOT FOUND") << "\n";
        log << "    Total found: " << outOffsets.GetFoundCount() << " (includes additional globals)\n";

        outLog = log.str();

        if (!outOffsets.IsValid())
            return { ScanResult::NoOffsetsFound, "No valid offsets found" };

        return { ScanResult::Success, "Scan completed successfully" };
    }
}
