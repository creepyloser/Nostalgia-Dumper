#include "dumper.h"
#include "patterns.h"
#include "memory.h"
#include "version.h"
#include <chrono>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

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

        std::string ToOffsetMacroKey(const std::string& key)
        {
            std::string out;
            out.reserve(key.size());
            for (char c : key)
            {
                if (std::isalnum(static_cast<unsigned char>(c)))
                    out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                else
                    out += '_';
            }
            return out;
        }

        void AppendJsonEscaped(std::ostringstream& o, const std::string& s)
        {
            for (unsigned char c : s)
            {
                switch (c)
                {
                    case '\\':
                        o << "\\\\";
                        break;
                    case '"':
                        o << "\\\"";
                        break;
                    case '\n':
                    case '\r':
                    case '\t':
                        o << ' ';
                        break;
                    default:
                        o << static_cast<char>(c);
                        break;
                }
            }
        }
    }

    Result ScanProcess(u32 processId, DiscoveredOffsets& outOffsets, std::string& outLog, bool exhaustiveUeSecondary)
    {
        std::ostringstream log;

        // Open process
        HANDLE processHandle = Memory::OpenProcessHandle(processId);
        if (!processHandle || processHandle == INVALID_HANDLE_VALUE)
        {
            log << "ERROR: Failed to open process (PID: " << processId << ")\n";
            outLog = log.str();
            return {ScanResult::CannotOpenProcess, "Cannot open process handle"};
        }

        log << "[*] Process opened successfully (PID: " << std::dec << processId << ")\n";

        std::vector<std::string> scanModules = Memory::GetUEScanModuleOrder(processHandle);
        if (scanModules.empty())
        {
            log << "[!] ERROR: No modules found in target process\n";
            Memory::CloseProcessHandle(processHandle);
            outLog = log.str();
            return {ScanResult::ModuleNotFound, "No modules in process"};
        }

        log << "[+] Scanning " << scanModules.size() << " module(s) (Engine.dll not required — order: UE/game exe + large DLLs)\n";
        const size_t kMaxLogMods = 24;
        for (size_t i = 0; i < scanModules.size() && i < kMaxLogMods; ++i)
            log << "    [" << i << "] " << scanModules[i] << "\n";
        if (scanModules.size() > kMaxLogMods)
            log << "    ... +" << (scanModules.size() - kMaxLogMods) << " more\n";
        if (exhaustiveUeSecondary)
            log << "\n[*] Deep UE scan: full kRipBulk + extra code sigs (slow)...\n";
        else
            log << "\n[*] UE scan: core + ProcessEvent + player chain (viewport / game instance / local player / "
                   "controller / pawn / game state) + bones / camera / mesh RIPs. Enable \"Deep UE scan\" for full bulk.\n";

        outOffsets = DiscoveredOffsets();
        Patterns::DiscoverAllOffsets(processHandle, scanModules, outOffsets, exhaustiveUeSecondary);
        ApplyDefaultMemberLayouts(outOffsets);

        auto logGlobal = [&](const char* name, const OffsetInfo& info) {
            if (info.address)
                log << "    [+] " << name << ": 0x" << std::hex << std::uppercase << info.address << std::dec << " (via " << info.signature << ")\n";
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
            << " estimated UObject/class member offsets (UE5.2–5.4 style — verify in IDA)\n";

        Memory::CloseProcessHandle(processHandle);

        // Summary
        log << "\n[*] Scan completed.\n";
        log << "\n[*] Results summary:\n";
        log << "    - GObjects:  " << (outOffsets.GObjects.address ? "FOUND" : "NOT FOUND") << "\n";
        log << "    - GNames:    " << (outOffsets.GNames.address ? "FOUND" : "NOT FOUND") << "\n";
        log << "    - GWorld:    " << (outOffsets.GWorld.address ? "FOUND" : "NOT FOUND") << "\n";
        log << "    - GEngine:   " << (outOffsets.GEngine.address ? "FOUND" : "NOT FOUND") << "\n";
        log << "    Total found: " << outOffsets.GetFoundCount() << " (includes additional globals)\n";

        outLog = log.str();

        if (!outOffsets.IsValid())
        {
            return {ScanResult::NoOffsetsFound, "No valid offsets found"};
        }

        return {ScanResult::Success, "Scan completed successfully"};
    }

    std::string GenerateHeaderContent(const DiscoveredOffsets& offsets, const std::string& gameName,
                                      const std::map<std::string, u64>* extraSymbols)
    {
        std::ostringstream header;

        // Get current time
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);

        char timeBuf[64] = {};
        if (ctime_s(timeBuf, sizeof(timeBuf), &time_t_now) != 0)
            timeBuf[0] = '\0';
        
        header << "#pragma once\n";
        header << "\n";
        header << "/**\n";
        header << " * @file offsets.h\n";
        header << " * @brief Unreal Engine 5 Offsets for " << gameName << "\n";
        header << " * @note Auto-generated by " << NOSTALGIA_TITLE << " v" << NOSTALGIA_VERSION_STR << "\n";
        header << " * @date " << timeBuf;
        header << " */\n";
        header << "\n";
        header << "#include <cstdint>\n";
        header << "#include <type_traits>\n";
        header << "\n";

        header << "namespace Offsets\n";
        header << "{\n";
        header << "    // Core globals\n";
        header << "    constexpr std::uintptr_t GObjects = 0x" << std::hex << std::uppercase << offsets.GObjects.address << ";\n";
        header << "    constexpr std::uintptr_t GNames = 0x" << std::hex << std::uppercase << offsets.GNames.address << ";\n";
        header << "    constexpr std::uintptr_t GWorld = 0x" << std::hex << std::uppercase << offsets.GWorld.address << ";\n";
        header << "    constexpr std::uintptr_t GEngine = 0x" << std::hex << std::uppercase << offsets.GEngine.address << ";\n";
        header << "\n";

        if (!offsets.customOffsets.empty())
        {
            header << "    // Additional globals\n";
            for (const auto& kv : offsets.customOffsets)
            {
                if (kv.second.address)
                    header << "    constexpr std::uintptr_t " << ToOffsetMacroKey(kv.first) << " = 0x" << std::hex << std::uppercase << kv.second.address << ";\n";
            }
            header << "\n";
        }

        if (extraSymbols && !extraSymbols->empty())
        {
            header << "    // Engine symbols\n";
            for (const auto& kv : *extraSymbols)
            {
                header << "    constexpr std::uintptr_t " << ToOffsetMacroKey(kv.first) << " = 0x" << std::hex << std::uppercase << kv.second << ";\n";
            }
            header << "\n";
        }

        header << "    // Member layout offsets\n";
        for (const auto& kv : offsets.layoutMemberOffsets)
        {
            header << "    constexpr std::uint32_t " << ToOffsetMacroKey(kv.first) << " = 0x" << std::hex << std::uppercase << kv.second << ";\n";
        }

        header << "} // namespace Offsets\n";
        return header.str();
    }

    std::string GenerateJsonContent(const DiscoveredOffsets& offsets, const std::string& gameName,
                                    const std::map<std::string, u64>* extraSymbols)
    {
        std::ostringstream json;
        json << "{\n";
        json << "  \"tool\": \"" << NOSTALGIA_TITLE << "\",\n";
        json << "  \"version\": \"" << NOSTALGIA_VERSION_STR << "\",\n";
        json << "  \"game\": \"" << gameName << "\",\n";
        json << "  \"offsets\": {\n";
        
        auto addOffset = [&](const char* key, u64 addr, bool last = false) {
            json << "    \"" << key << "\": \"0x" << std::hex << std::uppercase << addr << "\"" << (last ? "" : ",") << "\n";
        };

        addOffset("GObjects", offsets.GObjects.address);
        addOffset("GNames", offsets.GNames.address);
        addOffset("GWorld", offsets.GWorld.address);
        addOffset("GEngine", offsets.GEngine.address, (offsets.customOffsets.empty() && (!extraSymbols || extraSymbols->empty()) && offsets.layoutMemberOffsets.empty()));

        // ... (rest of JSON generation simplified for brevity)
        json << "  }\n";
        json << "}\n";
        return json.str();
    }
}
