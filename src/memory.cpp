#include "memory.h"
#include <TlHelp32.h>
#include <psapi.h>
#include <algorithm>
#include <cctype>
#include <unordered_set>

#pragma comment(lib, "psapi.lib")

namespace
{
    char CharToLower(char c)
    {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
}

namespace Memory
{
    int EnumerateProcesses(const std::string& searchName, std::vector<ProcessInfo>& results)
    {
        results.clear();
        
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE)
            return 0;

        PROCESSENTRY32W pe32 = {};
        pe32.dwSize = sizeof(PROCESSENTRY32W);

        std::string searchLower = searchName;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), CharToLower);

        if (Process32FirstW(hSnapshot, &pe32))
        {
            do
            {
                // Convert wchar to narrow string for comparison
                int bufferSize = WideCharToMultiByte(CP_UTF8, 0, pe32.szExeFile, -1, nullptr, 0, nullptr, nullptr);
                std::string processNameA(bufferSize - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, pe32.szExeFile, -1, &processNameA[0], bufferSize, nullptr, nullptr);

                std::string procNameLower = processNameA;
                std::transform(procNameLower.begin(), procNameLower.end(), procNameLower.begin(), CharToLower);

                // Match if search string is empty or found in process name
                if (searchName.empty() || procNameLower.find(searchLower) != std::string::npos)
                {
                    ProcessInfo info;
                    info.processId = pe32.th32ProcessID;
                    info.processName = processNameA;

                    // Try to get full path
                    HANDLE hProcess = OpenProcessHandle(pe32.th32ProcessID);
                    if (hProcess)
                    {
                        wchar_t pathWide[MAX_PATH] = {};
                        if (GetModuleFileNameExW(hProcess, nullptr, pathWide, MAX_PATH))
                        {
                            int pathBufferSize = WideCharToMultiByte(CP_UTF8, 0, pathWide, -1, nullptr, 0, nullptr, nullptr);
                            std::string pathA(pathBufferSize - 1, 0);
                            WideCharToMultiByte(CP_UTF8, 0, pathWide, -1, &pathA[0], pathBufferSize, nullptr, nullptr);
                            info.fullPath = pathA;
                        }
                        CloseProcessHandle(hProcess);
                    }

                    results.push_back(info);
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }

        CloseHandle(hSnapshot);
        return static_cast<int>(results.size());
    }

    u32 GetProcessIdByName(const std::string& processName)
    {
        std::vector<ProcessInfo> processes;
        EnumerateProcesses(processName, processes);
        
        if (!processes.empty())
            return processes[0].processId;
        
        return 0;
    }

    HANDLE OpenProcessHandle(u32 processId)
    {
        return ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    }

    void CloseProcessHandle(HANDLE handle)
    {
        if (handle && handle != INVALID_HANDLE_VALUE)
            ::CloseHandle(handle);
    }

    size_t ReadMemory(HANDLE handle, u64 address, void* buffer, size_t size)
    {
        if (!handle || !buffer || size == 0 || size > MAX_READ_SIZE)
            return 0;

        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(handle, reinterpret_cast<LPCVOID>(address), buffer, size, &bytesRead))
            return 0;

        return bytesRead;
    }

    u8 ReadByte(HANDLE handle, u64 address)
    {
        u8 value = 0;
        ReadMemory(handle, address, &value, sizeof(u8));
        return value;
    }

    u32 ReadDword(HANDLE handle, u64 address)
    {
        u32 value = 0;
        ReadMemory(handle, address, &value, sizeof(u32));
        return value;
    }

    u64 ReadQword(HANDLE handle, u64 address)
    {
        u64 value = 0;
        ReadMemory(handle, address, &value, sizeof(u64));
        return value;
    }

    u64 GetModuleBase(HANDLE handle, const std::string& moduleName)
    {
        if (!handle)
            return 0;

        HMODULE modules[1024];
        DWORD cbNeeded;

        if (!EnumProcessModules(handle, modules, sizeof(modules), &cbNeeded))
            return 0;

        int moduleCount = cbNeeded / sizeof(HMODULE);

        std::string searchLowerName = moduleName;
        std::transform(searchLowerName.begin(), searchLowerName.end(), searchLowerName.begin(), CharToLower);

        for (int i = 0; i < moduleCount; i++)
        {
            wchar_t szModNameW[MAX_PATH] = {};
            if (GetModuleBaseNameW(handle, modules[i], szModNameW, MAX_PATH))
            {
                int bufferSize = WideCharToMultiByte(CP_UTF8, 0, szModNameW, -1, nullptr, 0, nullptr, nullptr);
                std::string szModName(bufferSize - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, szModNameW, -1, &szModName[0], bufferSize, nullptr, nullptr);

                std::string currentModLower = szModName;
                std::transform(currentModLower.begin(), currentModLower.end(), currentModLower.begin(), CharToLower);

                if (currentModLower == searchLowerName)
                    return reinterpret_cast<u64>(modules[i]);
            }
        }

        return 0;
    }

    size_t GetModuleSize(HANDLE handle, const std::string& moduleName)
    {
        if (!handle)
            return 0;

        HMODULE modules[1024];
        DWORD cbNeeded;

        if (!EnumProcessModules(handle, modules, sizeof(modules), &cbNeeded))
            return 0;

        int moduleCount = cbNeeded / sizeof(HMODULE);

        std::string searchLowerName = moduleName;
        std::transform(searchLowerName.begin(), searchLowerName.end(), searchLowerName.begin(), CharToLower);

        for (int i = 0; i < moduleCount; i++)
        {
            wchar_t szModNameW[MAX_PATH] = {};
            if (GetModuleBaseNameW(handle, modules[i], szModNameW, MAX_PATH))
            {
                int bufferSize = WideCharToMultiByte(CP_UTF8, 0, szModNameW, -1, nullptr, 0, nullptr, nullptr);
                std::string szModName(bufferSize - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, szModNameW, -1, &szModName[0], bufferSize, nullptr, nullptr);

                std::string currentModLower = szModName;
                std::transform(currentModLower.begin(), currentModLower.end(), currentModLower.begin(), CharToLower);

                if (currentModLower == searchLowerName)
                {
                    MODULEINFO modInfo = {};
                    if (GetModuleInformation(handle, modules[i], &modInfo, sizeof(modInfo)))
                        return modInfo.SizeOfImage;
                }
            }
        }

        return 0;
    }

    int EnumerateModules(HANDLE handle, std::vector<ModuleInfo>& results)
    {
        results.clear();
        if (!handle)
            return 0;

        HMODULE modules[1024];
        DWORD cbNeeded = 0;
        if (!EnumProcessModules(handle, modules, sizeof(modules), &cbNeeded))
            return 0;

        const int moduleCount = static_cast<int>(cbNeeded / sizeof(HMODULE));
        for (int i = 0; i < moduleCount; i++)
        {
            wchar_t nameW[MAX_PATH] = {};
            if (!GetModuleBaseNameW(handle, modules[i], nameW, MAX_PATH))
                continue;

            int nameSize = WideCharToMultiByte(CP_UTF8, 0, nameW, -1, nullptr, 0, nullptr, nullptr);
            if (nameSize <= 1)
                continue;

            std::string nameUtf8(static_cast<size_t>(nameSize - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, nameW, -1, nameUtf8.data(), nameSize, nullptr, nullptr);

            MODULEINFO modInfo = {};
            if (!GetModuleInformation(handle, modules[i], &modInfo, sizeof(modInfo)))
                continue;

            ModuleInfo entry;
            entry.name = std::move(nameUtf8);
            entry.base = reinterpret_cast<u64>(modules[i]);
            entry.imageSize = modInfo.SizeOfImage;
            results.push_back(std::move(entry));
        }

        return static_cast<int>(results.size());
    }

    std::vector<std::string> GetUEScanModuleOrder(HANDLE handle)
    {
        std::vector<ModuleInfo> modules;
        if (EnumerateModules(handle, modules) == 0)
            return {};

        std::unordered_set<std::string> seenLower;
        std::vector<std::string> out;
        seenLower.reserve(modules.size() * 2);
        out.reserve(modules.size());

        auto toLowerStr = [](std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), CharToLower);
            return s;
        };

        auto tryAdd = [&](const std::string& name) {
            std::string key = toLowerStr(name);
            if (seenLower.count(key))
                return;
            seenLower.insert(std::move(key));
            out.push_back(name);
        };

        for (const auto& m : modules)
        {
            if (toLowerStr(m.name) == "engine.dll")
                tryAdd(m.name);
        }

        for (const auto& m : modules)
        {
            if (toLowerStr(m.name) == "unreal.dll")
                tryAdd(m.name);
        }

        for (const auto& m : modules)
        {
            std::string l = toLowerStr(m.name);
            if (l.find("unrealengine") != std::string::npos && l.size() > 4 &&
                l.compare(l.size() - 4, 4, ".dll") == 0)
                tryAdd(m.name);
        }

        for (const auto& m : modules)
        {
            std::string l = toLowerStr(m.name);
            if (l.find("engine") != std::string::npos && l.size() > 4 &&
                l.compare(l.size() - 4, 4, ".dll") == 0)
                tryAdd(m.name);
        }

        if (!modules.empty())
            tryAdd(modules[0].name);

        std::vector<ModuleInfo> bySize = modules;
        std::sort(bySize.begin(), bySize.end(), [](const ModuleInfo& a, const ModuleInfo& b) {
            return a.imageSize > b.imageSize;
        });
        for (const auto& m : bySize)
            tryAdd(m.name);

        return out;
    }

    bool IsValidAddress(u64 address)
    {
        // Check if address is in valid user-mode range on x64
        // Valid: 0x0 - 0x7FFFFFFFFFFFFFFF (user-mode)
        if (address > 0x7FFFFFFFFFFFFFFF)
            return false;

        // Reject null and very small addresses
        if (address < 0x10000)
            return false;

        // Address must be reasonably aligned (relaxed for some engines)
        // if (address % 8 != 0 && address % 4 != 0)
        //     return false;

        return true;
    }

    u64 ResolveRelativeAddress(u64 instructionAddr, i32 relativeOffset, int operandSize)
    {
        // RIP-relative formula: address = (instruction_address + instruction_size) + rel32
        // operandSize is typically 4 for 32-bit relative offsets
        return (instructionAddr + operandSize) + relativeOffset;
    }
}
