#pragma once
#include <Windows.h>
#include <winternl.h>
#include <cstdint>
#include <string>

// ---- Native API function pointers ----
typedef NTSTATUS(NTAPI* pNtReadVirtualMemory_t) (HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
typedef NTSTATUS(NTAPI* pNtWriteVirtualMemory_t)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
typedef NTSTATUS(NTAPI* pNtOpenProcess_t)       (PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, CLIENT_ID*);
typedef NTSTATUS(NTAPI* pNtDuplicateObject_t)   (HANDLE, HANDLE, HANDLE, PHANDLE, ACCESS_MASK, ULONG, ULONG);
typedef NTSTATUS(NTAPI* pNtQuerySystemInformation_t)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);

// ---- Global getters for syscall stubs ----
pNtReadVirtualMemory_t  GetNtReadVirtualMemory();
pNtWriteVirtualMemory_t GetNtWriteVirtualMemory();
pNtOpenProcess_t        GetNtOpenProcess();

// ---- Main Memory class ----
class Memory {
public:
    uint32_t  pid = 0;
    HANDLE    handle = nullptr;
    uintptr_t client = 0;
    uintptr_t engine = 0;

    // Initialises by trying handle hijacking first, then falling back to direct NtOpenProcess
    bool Init(const wchar_t* processName);

    void Cleanup();
    ~Memory() { Cleanup(); }

    // ---- Read / Write templates (with null‑pointer safety) ----
    template <typename T>
    T Read(uintptr_t address) const {
        T buffer{};
        if (address && handle) {
            auto fn = GetNtReadVirtualMemory();
            if (fn) {
                SIZE_T bytesRead = 0;
                fn(handle, (PVOID)address, &buffer, sizeof(T), &bytesRead);
            }
        }
        return buffer;
    }

    bool ReadRaw(uintptr_t address, void* buffer, size_t size) const;

    template <typename T>
    bool Write(uintptr_t address, T value) const {
        if (!address || !handle) return false;

        // Läs det nuvarande värdet i minnet först
        T currentValue = Read<T>(address);

        // Jämför råa bytes istället för att använda == operatorn
        // Detta förhindrar kompileringsfel för strukturer som Vector3, Matrix, etc.
        if (std::memcmp(&currentValue, &value, sizeof(T)) == 0) {
            return true;
        }

        auto fn = GetNtWriteVirtualMemory();
        if (!fn) return false;

        SIZE_T bytesWritten = 0;
        NTSTATUS status = fn(handle, (PVOID)address, &value, sizeof(T), &bytesWritten);
        return NT_SUCCESS(status) && bytesWritten == sizeof(T);
    }

    uintptr_t GetModuleBase(const wchar_t* moduleName) const;

private:
    uint32_t GetProcessId(const wchar_t* processName) const;

    // ---- Handle hijacking helpers ----
    HANDLE TryHijackHandle(DWORD targetPid);
    bool   IsTrustedSourceProcess(DWORD pid);
    std::wstring GetProcessNameByPid(DWORD pid);
};

extern Memory mem;