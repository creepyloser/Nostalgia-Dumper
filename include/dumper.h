#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "types.h"
#include <map>
#include <string>

/**
 * Dumper module - Orchestrates offset discovery and C++ header file generation
 * 
 * Coordinates memory scanning, pattern matching, and output generation.
 */
namespace Dumper
{
    /**
     * Main scanning orchestration function
     * Discovers all UE5 offsets from a running process
     * 
     * @param processId Process ID of target game
     * @param outOffsets Output structure with discovered offsets
     * @param outLog Output log string with scan details
     * @return Result code indicating success or failure
     */
    Result ScanProcess(u32 processId, DiscoveredOffsets& outOffsets, std::string& outLog,
                       bool exhaustiveUeSecondary = false);
    
    /**
     * Generate C++ header file content with offsets
     * Creates properly formatted #define macros for use in other projects
     * 
     * @param offsets The discovered offsets
     * @param gameName Name of the game (for header comments)
     * @return Header file content as string
     */
    std::string GenerateHeaderContent(const DiscoveredOffsets& offsets, const std::string& gameName = "Game",
                                      const std::map<std::string, u64>* extraSymbols = nullptr);
    
    /**
     * Write generated header to disk
     * 
     * @param offsets The discovered offsets
     * @param outputPath Full path where to write the file
     * @param gameName Name of the game (for comments)
     * @return Result indicating success or failure
     */
    Result WriteHeaderFile(const DiscoveredOffsets& offsets, const std::string& outputPath,
                           const std::string& gameName = "Game",
                           const std::map<std::string, u64>* extraSymbols = nullptr);

    /** C++ header with #define for each symbol (module bases, exports, IL2CPP API addresses). */
    std::string GenerateSymbolPackHeader(const std::map<std::string, u64>& symbols, const std::string& gameName,
                                         const std::string& engineTag);

    /** Minimal JSON: { "engine": "...", "process": "...", "symbols": { "NAME": "0x..." } } */
    std::string GenerateSymbolPackJson(const std::map<std::string, u64>& symbols, const std::string& engineTag,
                                       const std::string& gameName);

    /** Writes <fileStem>.h and <fileStem>.json into outputDirectory. */
    Result WriteSymbolPack(const std::map<std::string, u64>& symbols, const std::string& outputDirectory,
                           const std::string& fileStem, const std::string& gameName, const std::string& engineTag);
    
    /**
     * Validate offsets for reasonable values
     * Checks address ranges and pointer alignment
     * 
     * @param offsets Offsets to validate
     * @return true if all found offsets appear valid
     */
    bool ValidateOffsets(const DiscoveredOffsets& offsets);

    /**
     * Verifies found Unreal Engine globals by reading memory
     * Checks GUObjectArray count, GNames strings, GWorld levels
     * 
     * @param handle Process handle
     * @param offsets Discovered offsets
     * @param outDetails Detailed verification log
     * @return true if globals are verified and valid
     */
    bool VerifyUnrealGlobals(HANDLE handle, const DiscoveredOffsets& offsets, std::string& outDetails);
}
