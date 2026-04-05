#include "patterns.h"
#include "memory.h"
#include <algorithm>
#include <cctype>
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

    bool Scanner::IsValidPointer(u64 address) { return address > 0x10000 && address < 0x7FFFFFFFFFFF; }
    bool Scanner::IsValidOffset(u64 offset) { return offset > 0 && offset < 0xFFFFFFFF; }

    // Legacy wrappers
    BytePattern ParsePattern(const std::string& patternStr) { return Scanner::Parse(patternStr); }
    u64 ScanModule(HANDLE handle, const std::string& moduleName, const std::string& patternStr, int offset)
    {
        Scanner s(handle);
        return s.ScanModule(moduleName, patternStr, offset);
    }

    void DiscoverAllOffsets(HANDLE handle, const std::vector<std::string>& scanModuleNames, DiscoveredOffsets& out, bool exhaustiveSecondary)
    {
        Scanner s(handle);
        s.DiscoverUnrealOffsets(scanModuleNames, out, exhaustiveSecondary);
    }

    void Scanner::DiscoverUnrealOffsets(const std::vector<std::string>& modules, DiscoveredOffsets& out, [[maybe_unused]] bool deepScan)
    {
        const char* gobj[] = { "48 8B 05 ? ? ? ? 48 8B 0C C8 48 8B 01 FF 90", "48 8B 05 ? ? ? ? 48 8D 0C C8 48 8B 01" };
        const char* gnam[] = { "48 8D 05 ? ? ? ? EB 13 48 8D 0D", "48 8D 0D ? ? ? ? E8 ? ? ? ? 4C 8B C0 48 8D 0D" };
        const char* gwld[] = { "48 8B 05 ? ? ? ? 48 3B 42 18", "48 8B 1D ? ? ? ? 48 85 DB 74 3B" };
        const char* geng[] = { "48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ? 48 85 C9", "48 8B 0D ? ? ? ? 48 85 C9 0F 84" };

        auto scanCategory = [&](const char* /*label*/, const char* const* patterns, size_t count, OffsetInfo& primary, const char* heuristicStr = nullptr) {
            for (size_t i = 0; i < count; ++i) {
                u64 addr = ScanModule(modules[0], patterns[i], 3);
                if (addr) {
                    i32 rel = Memory::ReadDword(hProcess, addr);
                    primary.address = Memory::ResolveRelativeAddress(addr, rel);
                    primary.signature = patterns[i];
                    primary.moduleName = modules[0];
                    return;
                }
            }
            if (heuristicStr) {
                u64 addr = HeuristicScan(modules[0], heuristicStr);
                if (addr) {
                    i32 rel = Memory::ReadDword(hProcess, addr + 3);
                    primary.address = Memory::ResolveRelativeAddress(addr + 3, rel);
                    primary.signature = "HEURISTIC_FALLBACK";
                    primary.moduleName = modules[0];
                }
            }
        };

        scanCategory("GObjects", gobj, 2, out.GObjects, "GObjectArray");
        scanCategory("GNames", gnam, 2, out.GNames, "FNamePool");
        scanCategory("GWorld", gwld, 2, out.GWorld, "GWorld");
        scanCategory("GEngine", geng, 2, out.GEngine, "GEngine");
    }
}
