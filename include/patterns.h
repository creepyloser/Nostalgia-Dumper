#pragma once

#include "types.h"
#include <windows.h>
#include <map>
#include <string>
#include <vector>
#include <memory>

namespace Patterns
{
    // Max scan size per module (256MB)
    constexpr size_t MAX_SCAN_SIZE = 256 * 1024 * 1024;
    
    struct BytePattern
    {
        std::vector<u8> bytes;
        std::vector<u8> mask;
        std::string original;

        bool isValid() const { return !bytes.empty(); }
    };
    
    /**
     * Core scanning engine
     */
    class Scanner
    {
    public:
        explicit Scanner(HANDLE processHandle);
        ~Scanner() = default;

        // Pattern parsing
        static BytePattern Parse(const std::string& patternStr);
        
        // Basic scanning
        u64 Scan(u64 startAddr, size_t scanSize, const BytePattern& pattern, int offset = 0);
        u64 ScanModule(const std::string& moduleName, const std::string& patternStr, int offset = 0);

        // Advanced discovery
        void DiscoverUnrealOffsets(const std::vector<std::string>& modules, DiscoveredOffsets& out, bool deepScan = false);
        
        // Heuristic fallback
        u64 HeuristicScan(const std::string& moduleName, const std::string& stringToFind);

        // Validation helpers
        bool IsValidPointer(u64 address);
        bool IsValidOffset(u64 offset);

    private:
        HANDLE hProcess;
        
        struct ModuleCache {
            u64 base = 0;
            size_t size = 0;
            std::vector<u8> data;
        };
        std::map<std::string, std::shared_ptr<ModuleCache>> cache;

        std::shared_ptr<ModuleCache> GetModule(const std::string& name);
    };

    // Legacy compatibility wrappers (to avoid breaking existing code immediately)
    BytePattern ParsePattern(const std::string& patternStr);
    u64 ScanModule(HANDLE handle, const std::string& moduleName, const std::string& patternStr, int offset = 0);
    void DiscoverAllOffsets(HANDLE handle, const std::vector<std::string>& scanModuleNames, DiscoveredOffsets& out, bool exhaustiveSecondary = false);
}
