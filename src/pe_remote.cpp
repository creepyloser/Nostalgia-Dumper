#include "pe_remote.h"
#include "memory.h"
#include <vector>
#include <winnt.h>

namespace
{
    bool ReadRemoteString(HANDLE h, u64 addr, std::string& out, size_t maxLen = 512)
    {
        out.clear();
        if (maxLen == 0 || maxLen > 4096)
            maxLen = 512;
        std::vector<char> buf(maxLen);
        const size_t n = Memory::ReadMemory(h, addr, buf.data(), buf.size());
        if (n == 0)
            return false;
        size_t len = 0;
        while (len < n && buf[len] != '\0')
            ++len;
        out.assign(buf.data(), len);
        return !out.empty();
    }
}

namespace PeRemote
{
    bool EnumerateExports(HANDLE process, u64 imageBase, std::vector<ExportEntry>& out)
    {
        out.clear();
        if (!process || imageBase == 0)
            return false;

        IMAGE_DOS_HEADER dos{};
        if (Memory::ReadMemory(process, imageBase, &dos, sizeof(dos)) != sizeof(dos))
            return false;
        if (dos.e_magic != IMAGE_DOS_SIGNATURE)
            return false;

        const u64 ntVa = imageBase + static_cast<u32>(dos.e_lfanew);
        IMAGE_NT_HEADERS64 nt{};
        if (Memory::ReadMemory(process, ntVa, &nt, sizeof(nt)) != sizeof(nt))
            return false;
        if (nt.Signature != IMAGE_NT_SIGNATURE)
            return false;
        if (nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            return false;

        const u32 exportRva = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
        const u32 exportSize = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
        if (exportRva == 0 || exportSize == 0)
            return true;

        const u64 exportDirVa = imageBase + exportRva;
        IMAGE_EXPORT_DIRECTORY ed{};
        if (Memory::ReadMemory(process, exportDirVa, &ed, sizeof(ed)) != sizeof(ed))
            return false;

        const u32 numNames = ed.NumberOfNames;
        if (numNames == 0)
            return true;

        const u64 namesVa = imageBase + ed.AddressOfNames;
        const u64 ordinalsVa = imageBase + ed.AddressOfNameOrdinals;
        const u64 funcsVa = imageBase + ed.AddressOfFunctions;

        out.reserve(static_cast<size_t>(numNames));

        for (u32 i = 0; i < numNames; ++i)
        {
            const u32 nameRva = Memory::ReadDword(process, namesVa + static_cast<u64>(i) * 4u);
            if (nameRva == 0)
                continue;

            u16 ordIndex = 0;
            if (Memory::ReadMemory(process, ordinalsVa + static_cast<u64>(i) * 2u, &ordIndex, sizeof(ordIndex)) !=
                sizeof(ordIndex))
                continue;
            if (ordIndex >= ed.NumberOfFunctions)
                continue;

            const u32 funcRva = Memory::ReadDword(process, funcsVa + static_cast<u64>(ordIndex) * 4u);
            if (funcRva == 0)
                continue;

            if (funcRva >= exportRva && funcRva < exportRva + exportSize)
                continue;

            std::string symName;
            if (!ReadRemoteString(process, imageBase + nameRva, symName, 512))
                continue;

            ExportEntry e;
            e.name = std::move(symName);
            e.rva = funcRva;
            e.va = imageBase + funcRva;
            out.push_back(std::move(e));
        }

        return true;
    }

    bool FindExportVa(HANDLE process, u64 imageBase, const char* name, u64& outVa)
    {
        outVa = 0;
        if (!name || !name[0])
            return false;

        std::vector<ExportEntry> all;
        if (!EnumerateExports(process, imageBase, all))
            return false;

        for (const auto& e : all)
        {
            if (e.name == name)
            {
                outVa = e.va;
                return true;
            }
        }
        return false;
    }
}
