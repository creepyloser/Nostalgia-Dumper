#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>

// Basic type aliases for clarity
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

// Process information structure
struct ProcessInfo
{
    u32 processId;
    std::string processName;
    std::string fullPath;
};

// Discovered offsets structure
struct OffsetInfo
{
    u64 address = 0;
    std::string signature;
    std::string moduleName;
};

struct DiscoveredOffsets
{
    OffsetInfo GObjects;           // GUObjectArray
    OffsetInfo GNames;            // FNamePool
    OffsetInfo GWorld;            // UWorld pointer
    OffsetInfo GEngine;           // UEngine singleton
    OffsetInfo FNamePool;         // Additional FName pool
    OffsetInfo GUObjectArray;     // Explicit GUObjectArray
    
    // Custom/additional offsets
    std::map<std::string, OffsetInfo> customOffsets;

    // Typical UObject / UStruct member offsets (UE5.2–5.4 x64 estimates — verify in IDA)
    std::map<std::string, u64> layoutMemberOffsets;

    /** UTF-8 paths like /Script/Engine.Actor found in scanned modules (reflection hints). */
    std::vector<std::string> scriptPathHints;
    /** Best-effort UObject samples: "index=N obj=0x.. class=0x.." (names need separate FName decode). */
    std::vector<std::string> uobjectSamples;
    
    // Check if at least one offset was found
    inline bool IsValid() const
    {
        if (GObjects.address != 0 || GNames.address != 0 || GWorld.address != 0 || GEngine.address != 0 || 
            FNamePool.address != 0 || GUObjectArray.address != 0)
            return true;
        for (const auto& kv : customOffsets)
        {
            if (kv.second.address != 0)
                return true;
        }
        return false;
    }
    
    // Get count of found offsets
    inline int GetFoundCount() const
    {
        int count = 0;
        if (GObjects.address) count++;
        if (GNames.address) count++;
        if (GWorld.address) count++;
        if (GEngine.address) count++;
        if (FNamePool.address) count++;
        if (GUObjectArray.address) count++;
        return count + static_cast<int>(customOffsets.size()) + static_cast<int>(layoutMemberOffsets.size());
    }
};

// Scanning result
enum class ScanResult
{
    Success = 0,
    ProcessNotFound = -1,
    CannotOpenProcess = -2,
    ModuleNotFound = -3,
    InvalidAddress = -4,
    MemoryReadFailed = -5,
    NoOffsetsFound = -6,
    FileWriteFailed = -7,
};

// Result status with message
struct Result
{
    ScanResult code;
    std::string message;
    
    bool IsSuccess() const { return code == ScanResult::Success; }
    std::string ToString() const { return message; }
};
