#pragma once

#include "types.h"
#include <string>
#include <vector>
#include <windows.h>

/**
 * Read PE export tables from a module mapped in another process.
 * All VAs are absolute runtime addresses (image base + RVA).
 */
namespace PeRemote
{
    struct ExportEntry
    {
        std::string name;
        u32 rva = 0;
        u64 va = 0;
    };

    /** List every named export (skips forwarders). Returns false on PE read failure. */
    bool EnumerateExports(HANDLE process, u64 imageBase, std::vector<ExportEntry>& out);

    /** O(n) over export names; use after EnumerateExports if many lookups needed. */
    bool FindExportVa(HANDLE process, u64 imageBase, const char* name, u64& outVa);
}
