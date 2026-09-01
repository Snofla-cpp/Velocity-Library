#include "Overlay.h"
#include "Memory.h"
#include "CallStack-Spoofer.h"
#include "xorstr.hpp"

#include <Windows.h>

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    SetProcessDPIAware();

    // Hide from debugger
    {
        typedef NTSTATUS(NTAPI* pNtSetInformationThread)(HANDLE, ULONG, PVOID, ULONG);
        auto NtSetInformationThread = (pNtSetInformationThread)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtSetInformationThread");
        if (NtSetInformationThread)
            NtSetInformationThread(GetCurrentThread(), 0x11, nullptr, 0);
    }



    // Wait for game and init memory
    while (!FindWindowA(nullptr, xorstr_("Counter-Strike 2"))) Sleep(200);
    while (!mem.Init(xorstr_(L"cs2.exe"))) Sleep(500);

    // Create overlay and run
    if (!Overlay::Create()) return 1;
    Overlay::Run();
    Overlay::Destroy();



    return 0;
}