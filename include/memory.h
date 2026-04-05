#pragma once

#include "types.h"
#include <windows.h>
#include <string>
#include <vector>

/**
 * Memory module - Handles all Windows process memory access operations
 * 
 * Provides abstraction for:
 * - Process enumeration
 * - Memory reading from external processes
 * - Module discovery and information
 * - Safe memory access with validation
 */
/** One loaded module in a process (short name, base, image size). */
struct ModuleInfo
{
    std::string name;
    u64 base;
    size_t imageSize;
};

namespace Memory
{
    // Constants for memory access
    constexpr size_t MEMORY_CHUNK_SIZE = 1024 * 1024;  // 1MB chunks for large reads
    constexpr size_t MAX_READ_SIZE = 512 * 1024 * 1024; // 512MB max single read
    
    /**
     * Enumerate all running processes
     * @param searchName Optional filter by executable name (case-insensitive substring match)
     * @param results Output vector of matching processes
     * @return Number of processes found
     */
    int EnumerateProcesses(const std::string& searchName, std::vector<ProcessInfo>& results);

    /**
     * List all modules loaded in the process (exe + DLLs).
     * @return Number of modules listed
     */
    int EnumerateModules(HANDLE handle, std::vector<ModuleInfo>& results);

    /**
     * Module short names in priority order for UE signature scans (Engine, Unreal*, largest images, etc.).
     */
    std::vector<std::string> GetUEScanModuleOrder(HANDLE handle);
    
    /**
     * Get process ID by name
     * @param processName Name of the process executable
     * @return Process ID, or 0 if not found
     */
    u32 GetProcessIdByName(const std::string& processName);
    
    /**
     * Open a handle to a process with read permissions
     * @param processId The process ID to open
     * @return Valid HANDLE if successful, INVALID_HANDLE_VALUE on failure
     */
    HANDLE OpenProcessHandle(u32 processId);
    
    /**
     * Close a process handle safely
     * @param handle The handle to close
     */
    void CloseProcessHandle(HANDLE handle);
    
    /**
     * Read arbitrary memory from a process
     * @param handle Open process handle
     * @param address Memory address to read from
     * @param buffer Output buffer to store data
     * @param size Number of bytes to read
     * @return Number of bytes actually read (may be less than requested)
     */
    size_t ReadMemory(HANDLE handle, u64 address, void* buffer, size_t size);
    
    /**
     * Read a single byte
     * @param handle Process handle
     * @param address Address to read from
     * @return Byte value
     */
    u8 ReadByte(HANDLE handle, u64 address);
    
    /**
     * Read a 32-bit value
     * @param handle Process handle
     * @param address Address to read from
     * @return DWORD value
     */
    u32 ReadDword(HANDLE handle, u64 address);
    
    /**
     * Read a 64-bit pointer/qword
     * @param handle Process handle
     * @param address Address to read from
     * @return QWORD value
     */
    u64 ReadQword(HANDLE handle, u64 address);
    
    /**
     * Get the base address of a loaded module
     * @param handle Process handle
     * @param moduleName Module name (e.g., "Engine.dll")
     * @return Base address of module, 0 if not found
     */
    u64 GetModuleBase(HANDLE handle, const std::string& moduleName);
    
    /**
     * Get the size (in bytes) of a loaded module
     * @param handle Process handle
     * @param moduleName Module name
     * @return Module size in bytes, 0 if not found
     */
    size_t GetModuleSize(HANDLE handle, const std::string& moduleName);
    
    /**
     * Validate an address as a reasonable process memory location
     * @param address Address to validate
     * @return true if address appears to be valid
     */
    bool IsValidAddress(u64 address);
    
    /**
     * Resolve a relative address (RIP-relative in x64 encoding)
     * Typically used for lea operations: lea rax, [rip + rel32]
     * 
     * @param instructionAddr Address of the instruction
     * @param relativeOffset The relative offset (usually 4 bytes signed int32)
     * @param operandSize Size of operand (usually 4 for 32-bit relative offsets)
     * @return Absolute resolved address
     */
    u64 ResolveRelativeAddress(u64 instructionAddr, i32 relativeOffset, int operandSize = 4);
}
