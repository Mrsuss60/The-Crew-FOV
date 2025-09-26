#include "pch.h"
#pragma comment(linker, "/export:PdcActivationClientRegister=TheCrewFOV.PdcActivationClientRegister,@3")
#pragma comment(lib, "user32.lib")

static DWORD WINAPI LoadFOVDLL(LPVOID _param) {

    Sleep(2000);

    HMODULE hMod = LoadLibraryA("TheCrewFOV.dll");

    return 0;
}

static BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD ul_reason_for_call,
    LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule); 
        CreateThread(NULL, 0, LoadFOVDLL, NULL, 0, NULL); 
        break; 
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}