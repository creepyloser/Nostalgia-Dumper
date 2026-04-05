#include "patterns.h"
#include "memory.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iterator>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <thread>
#include <future>

namespace Patterns
{
    Scanner::Scanner(HANDLE processHandle) : hProcess(processHandle) {}

    BytePattern Scanner::Parse(const std::string& patternStr)
    {
        BytePattern pattern;
        pattern.original = patternStr;
        
        std::istringstream iss(patternStr);
        std::string token;
        while (iss >> token)
        {
            if (token == "?" || token == "??")
            {
                pattern.bytes.push_back(0);
                pattern.mask.push_back(0x00);
            }
            else
            {
                try
                {
                    u8 byte = static_cast<u8>(std::stoul(token, nullptr, 16));
                    pattern.bytes.push_back(byte);
                    pattern.mask.push_back(0xFF);
                }
                catch (...)
                {
                    return {};
                }
            }
        }
        return pattern;
    }

    std::shared_ptr<Scanner::ModuleCache> Scanner::GetModule(const std::string& name)
    {
        auto it = cache.find(name);
        if (it != cache.end())
            return it->second;

        u64 base = Memory::GetModuleBase(hProcess, name);
        if (base == 0) return nullptr;

        size_t size = Memory::GetModuleSize(hProcess, name);
        if (size == 0) return nullptr;

        size_t scanSize = std::min(size, MAX_SCAN_SIZE);
        auto mod = std::make_shared<ModuleCache>();
        mod->base = base;
        mod->size = scanSize;
        mod->data.resize(scanSize);

        size_t bytesRead = Memory::ReadMemory(hProcess, base, mod->data.data(), scanSize);
        if (bytesRead < 4) return nullptr;

        mod->data.resize(bytesRead);
        cache[name] = mod;
        return mod;
    }

    u64 Scanner::Scan(u64 startAddr, size_t scanSize, const BytePattern& pattern, int offset)
    {
        if (!pattern.isValid() || scanSize < pattern.bytes.size())
            return 0;

        std::vector<u8> buffer(scanSize);
        size_t bytesRead = Memory::ReadMemory(hProcess, startAddr, buffer.data(), scanSize);
        if (bytesRead < pattern.bytes.size())
            return 0;

        for (size_t i = 0; i <= bytesRead - pattern.bytes.size(); ++i)
        {
            bool match = true;
            for (size_t j = 0; j < pattern.bytes.size(); ++j)
            {
                if ((buffer[i + j] & pattern.mask[j]) != (pattern.bytes[j] & pattern.mask[j]))
                {
                    match = false;
                    break;
                }
            }
            if (match)
                return startAddr + i + offset;
        }
        return 0;
    }

    u64 Scanner::ScanModule(const std::string& moduleName, const std::string& patternStr, int offset)
    {
        auto mod = GetModule(moduleName);
        if (!mod) return 0;

        BytePattern pattern = Parse(patternStr);
        if (!pattern.isValid()) return 0;

        const u8* data = mod->data.data();
        size_t size = mod->data.size();

        // Multi-threaded scan implementation
        unsigned int numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 2;
        
        std::vector<std::future<u64>> futures;
        size_t chunkSize = size / numThreads;

        for (unsigned int t = 0; t < numThreads; ++t) {
            size_t start = t * chunkSize;
            size_t end = (t == numThreads - 1) ? size : (t + 1) * chunkSize + pattern.bytes.size();
            
            futures.push_back(std::async(std::launch::async, [this, data, start, end, &pattern, mod, offset]() -> u64 {
                for (size_t i = start; i <= end - pattern.bytes.size() && i < mod->data.size(); ++i) {
                    bool match = true;
                    for (size_t j = 0; j < pattern.bytes.size(); ++j) {
                        if ((data[i + j] & pattern.mask[j]) != (pattern.bytes[j] & pattern.mask[j])) {
                            match = false;
                            break;
                        }
                    }
                    if (match) return mod->base + i + offset;
                }
                return 0;
            }));
        }

        for (auto& f : futures) {
            u64 res = f.get();
            if (res) return res;
        }

        return 0;
    }

    u64 Scanner::HeuristicScan(const std::string& moduleName, const std::string& stringToFind)
    {
        auto mod = GetModule(moduleName);
        if (!mod) return 0;

        const u8* data = mod->data.data();
        size_t size = mod->data.size();

        // 1. Find the string in memory
        auto it = std::search(data, data + size, stringToFind.begin(), stringToFind.end());
        if (it == data + size) return 0;

        u64 stringVa = mod->base + (it - data);

        // 2. Find references to this string (LEA/MOV)
        for (size_t i = 0; i < size - 7; ++i) {
            if (data[i] == 0x48 && data[i+1] == 0x8D) { // LEA
                i32 rel = *reinterpret_cast<const i32*>(&data[i+3]);
                if (mod->base + i + 7 + rel == stringVa) {
                    return mod->base + i;
                }
            }
        }
        return 0;
    }

    u64 Scanner::HeuristicMovRipToString(const std::string& moduleName, const std::string& stringToFind)
    {
        auto mod = GetModule(moduleName);
        if (!mod)
            return 0;

        const u8* data = mod->data.data();
        const size_t size = mod->data.size();

        auto it = std::search(data, data + size, stringToFind.begin(), stringToFind.end());
        if (it == data + size)
            return 0;

        const u64 stringVa = mod->base + static_cast<u64>(it - data);

        for (size_t i = 0; i + 7 < size; ++i) {
            if (data[i] != 0x48 || data[i + 1] != 0x8B)
                continue;
            const u8 rm = data[i + 2];
            if (rm != 0x05 && rm != 0x0D && rm != 0x15 && rm != 0x1D)
                continue;
            const i32 rel = *reinterpret_cast<const i32*>(&data[i + 3]);
            const u64 next = mod->base + i + 7;
            if (next + static_cast<i64>(rel) == stringVa)
                return mod->base + i;
        }
        return 0;
    }

    bool Scanner::IsValidPointer(u64 address) { return address > 0x10000 && address < 0x7FFFFFFFFFFF; }
    bool Scanner::IsValidOffset(u64 offset) { return offset > 0 && offset < 0xFFFFFFFF; }

    // Legacy wrappers
    BytePattern ParsePattern(const std::string& patternStr) { return Scanner::Parse(patternStr); }
    u64 ScanModule(HANDLE handle, const std::string& moduleName, const std::string& patternStr, int offset)
    {
        Scanner s(handle);
        return s.ScanModule(moduleName, patternStr, offset);
    }

    namespace
    {
        // UE4/UE5 x64 — order: common UE5 first, then UE4 / older variants. Wildcards: ? = any byte.
        static const char* const kGObjects[] = {
            "48 8B 05 ? ? ? ? 48 8B 0C C8 48 8B 01 FF 90",
            "48 8B 05 ? ? ? ? 48 8D 0C C8 48 8B 01",
            "48 8B 05 ? ? ? ? 48 8B 0C C8 48 8B 04 D1 EB",
            "48 8B 05 ? ? ? ? 48 8B 0C C8 48 8B 04 D1",
            "48 8B 05 ? ? ? ? 48 8B 0C D0 48 8B 01",
            "48 8B 0D ? ? ? ? 48 8B 04 D1 48 85 C0 75",
            "48 8B 05 ? ? ? ? 4C 8D 34 C8 48 89 5C 24",
            "48 8B 05 ? ? ? ? 48 8B 0C C8 48 8B 01 48 85 C0",
            "48 8B 05 ? ? ? ? 48 8B 34 C8 48 85 F6",
            "48 8B 15 ? ? ? ? 48 8B 0C EA 48 8B 04 C1",
        };
        static const char* const kGObjectsDeep[] = {
            "48 8B 05 ? ? ? ? 48 8B 0C C8 48 8B 04 D1 48 85 C0",
            "48 8B 05 ? ? ? ? 8B 0C 85 ? ? ? ? 48 8B 14",
            "48 8B 05 ? ? ? ? 48 63 ? 48 8B 0C",
        };

        static const char* const kGNames[] = {
            "48 8D 0D ? ? ? ? EB ? 48 8B D0",
            "48 8D 0D ? ? ? ? E8 ? ? ? ? 4C 8B C0 48 8D 0D",
            "48 8D 05 ? ? ? ? EB 13 48 8D 0D",
            "48 8D 0D ? ? ? ? E8 ? ? ? ? 48 8B D8",
            "48 8B 05 ? ? ? ? 48 85 C0 75 ? 39 05",
            "48 8D 15 ? ? ? ? 33 C0 48 89 41 30",
            "48 8D 05 ? ? ? ? EB 16 48 8B CB",
            "48 8D 0D ? ? ? ? 0F B6 01 84 C0 74",
            "4C 8D 25 ? ? ? ? 49 39 24 24 75",
            "48 8D 0D ? ? ? ? 48 8B 01 48 85 C0",
        };
        static const char* const kGNamesDeep[] = {
            "48 8D 0D ? ? ? ? 48 8B 89 ? ? ? ? 48 85 C9",
            "48 8B 05 ? ? ? ? 48 8B D0 48 8B 0D",
            "48 8D 35 ? ? ? ? 80 7C 24",
        };

        static const char* const kGWorld[] = {
            "48 8B 1D ? ? ? ? 48 85 DB 74 3B 41 B0 01",
            "48 8B 1D ? ? ? ? 48 85 DB 74 ? 41 B0 01",
            "48 8B 05 ? ? ? ? 48 3B 42 18",
            "48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ? 48 85 C9 74 ? 48 8B 01",
            "48 8B 15 ? ? ? ? 4C 8B 14 52 48 8B 0C C2",
            "48 8B 0D ? ? ? ? 48 85 C9 74 ? E8 ? ? ? ?",
            "48 8B 05 ? ? ? ? F2 0F 10 40 ? F3 0F 10 88",
            "48 8B 1D ? ? ? ? 48 85 DB 0F 84",
            "48 8B 05 ? ? ? ? 48 8B B8 ? ? ? ? 48 85 FF",
            "48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01",
        };
        static const char* const kGWorldDeep[] = {
            "48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ? 48 85 C9 74 ? 48 8B 49",
            "48 8B 1D ? ? ? ? 48 8B 43 30 48 85 C0",
        };

        static const char* const kGEngine[] = {
            "48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ? 48 85 C9",
            "48 8B 0D ? ? ? ? 48 85 C9 0F 84",
            "48 8B 05 ? ? ? ? 48 8B D0 48 8B 88 ? ? ? ? 48 85 C9",
            "48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ? 48 85 C9 74 ? 48 8B 01",
            "48 8B 05 ? ? ? ? 48 63 ? 48 8B 0C",
            "40 53 48 83 EC 20 48 8B D9 48 8B 05 ? ? ? ? 48 85 C0",
            "48 8B 05 ? ? ? ? 48 8B 98 ? ? ? ? 48 85 DB",
            "48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B C8",
            "48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ? 48 85 C9 74 ? E8",
        };
        static const char* const kGEngineDeep[] = {
            "48 8B 05 ? ? ? ? F3 0F 7F 80 ? ? ? ? 48 8B",
            "48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ? 48 85 C9 74 ? 48 8B 49",
        };

        static const char* const kProcessEvent[] = {
            "40 55 57 41 54 41 55 41 57 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85",
            "40 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 45",
            "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC 50 48 8B F9 48 8B 0D ? ? ? ? 48 85 C9",
        };

        template <size_t N>
        constexpr size_t Count(const char* const (&)[N])
        {
            return N;
        }
    }

    void DiscoverAllOffsets(HANDLE handle, const std::vector<std::string>& scanModuleNames, DiscoveredOffsets& out, bool exhaustiveSecondary)
    {
        Scanner s(handle);
        s.DiscoverUnrealOffsets(scanModuleNames, out, exhaustiveSecondary);
    }

    void Scanner::DiscoverUnrealOffsets(const std::vector<std::string>& modules, DiscoveredOffsets& out, bool deepScan)
    {
        auto tryResolveRip = [&](u64 ripDispAddr) -> u64 {
            const i32 rel = Memory::ReadDword(hProcess, ripDispAddr);
            const u64 resolved = Memory::ResolveRelativeAddress(ripDispAddr, rel, 4);
            if (!Memory::IsValidAddress(resolved))
                return 0;
            return resolved;
        };

        auto scanCategory = [&](const char* const* patterns, size_t count, OffsetInfo& primary,
                                const char* heuristicStr = nullptr, const char* heuristicAlt = nullptr) {
            for (const auto& modName : modules) {
                for (size_t i = 0; i < count; ++i) {
                    const u64 addr = ScanModule(modName, patterns[i], 3);
                    if (!addr)
                        continue;
                    const u64 resolved = tryResolveRip(addr);
                    if (!resolved)
                        continue;
                    primary.address = resolved;
                    primary.signature = patterns[i];
                    primary.moduleName = modName;
                    return;
                }
                if (heuristicStr) {
                    u64 lea = HeuristicScan(modName, heuristicStr);
                    if (!lea)
                        lea = HeuristicMovRipToString(modName, heuristicStr);
                    if (lea) {
                        const u64 resolved = tryResolveRip(lea + 3);
                        if (resolved) {
                            primary.address = resolved;
                            primary.signature = "HEURISTIC_LEA_MOV";
                            primary.moduleName = modName;
                            return;
                        }
                    }
                }
                if (heuristicAlt && std::strcmp(heuristicAlt, heuristicStr) != 0) {
                    u64 lea = HeuristicScan(modName, heuristicAlt);
                    if (!lea)
                        lea = HeuristicMovRipToString(modName, heuristicAlt);
                    if (lea) {
                        const u64 resolved = tryResolveRip(lea + 3);
                        if (resolved) {
                            primary.address = resolved;
                            primary.signature = "HEURISTIC_LEA_MOV_ALT";
                            primary.moduleName = modName;
                            return;
                        }
                    }
                }
            }
        };

        scanCategory(kGObjects, Count(kGObjects), out.GObjects, "GUObjectArray", "GObjectArray");
        scanCategory(kGNames, Count(kGNames), out.GNames, "FNamePool", "GNames");
        scanCategory(kGWorld, Count(kGWorld), out.GWorld, "GWorld", "World");
        scanCategory(kGEngine, Count(kGEngine), out.GEngine, "GEngine", "Engine");

        if (deepScan) {
            if (!out.GObjects.address)
                scanCategory(kGObjectsDeep, Count(kGObjectsDeep), out.GObjects, "GUObjectArray", "ChunkedFixedUObjectArray");
            if (!out.GNames.address)
                scanCategory(kGNamesDeep, Count(kGNamesDeep), out.GNames, "FNamePool", "NamePool");
            if (!out.GWorld.address)
                scanCategory(kGWorldDeep, Count(kGWorldDeep), out.GWorld, "GWorld", "PersistentLevel");
            if (!out.GEngine.address)
                scanCategory(kGEngineDeep, Count(kGEngineDeep), out.GEngine, "GEngine", "GameEngine");
        }

        int peIdx = 0;
        for (const auto& modName : modules) {
            for (size_t i = 0; i < Count(kProcessEvent); ++i) {
                const u64 hit = ScanModule(modName, kProcessEvent[i], 0);
                if (!hit)
                    continue;
                std::string key = "UObject_ProcessEvent_Candidate";
                if (peIdx > 0)
                    key += "_" + std::to_string(peIdx);
                out.customOffsets[key].address = hit;
                out.customOffsets[key].signature = kProcessEvent[i];
                out.customOffsets[key].moduleName = modName;
                ++peIdx;
                if (peIdx >= 3)
                    goto pe_done;
            }
        }
    pe_done:;
    }
}
