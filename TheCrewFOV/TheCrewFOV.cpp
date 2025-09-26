#include "pch.h"


const float g_RenderMin = 0.01f;
const float g_RenderMax = 20000.0f;
const float g_CameraSpeed = 125.0f;

HMODULE   g_gameModule = nullptr;
uintptr_t g_baseAddress = 0;
uintptr_t g_cameraStructAddr = 0;
bool      g_initialized = false;


std::vector<BYTE> g_pattern = {
    0x48, 0x8B, 0x0D, 0x00,0x00,0x00,0x00,
    0x48, 0x8D, 0x15, 0x00,0x00,0x00,0x00,
    0x45, 0x33, 0xC0,
    0xE8, 0x00,0x00,0x00,0x00,
    0x48, 0x8B, 0x0D, 0x00,0x00,0x00,0x00,
    0x48, 0x85
};
std::vector<BYTE> g_mask = {
    0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,
    0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,
    0xFF,0xFF,0xFF,
    0xFF,0x00,0x00,0x00,0x00,
    0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,
    0xFF,0xFF
};

static uintptr_t FindPattern() {
    MODULEINFO mi;
    if (!GetModuleInformation(GetCurrentProcess(), g_gameModule, &mi, sizeof(mi)))
        return 0;

    uintptr_t base = (uintptr_t)g_gameModule;
    SIZE_T size = mi.SizeOfImage;

    for (uintptr_t i = base; i < base + size - g_pattern.size(); i++) {
        bool found = true;
        for (size_t j = 0; j < g_pattern.size(); j++) {
            if (g_mask[j] == 0xFF) {
                BYTE b;
                if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(i + j), &b, 1, nullptr) || b != g_pattern[j]) {
                    found = false; break;
                }
            }
        }
        if (found) return i;
    }
    return 0;
}

static bool Initialize() {
    g_gameModule = GetModuleHandleA("TheCrew.exe");
    if (!g_gameModule) return false;

    uintptr_t addr = FindPattern();
    if (!addr) return false;

    DWORD rel;
    if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(addr + 3), &rel, sizeof(rel), nullptr))
        return false;

    g_baseAddress = addr + rel + 7;
    g_initialized = true;
    return true;
}

static bool UpdateCameraAddress() {
    if (!g_initialized || g_baseAddress == 0) return false;

    uintptr_t cur = g_baseAddress;
    if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)cur, &cur, sizeof(cur), nullptr)) return false;

    std::vector<uintptr_t> offsets = { 0x1B0,0x5A8,0x40,0x448,0x0 };
    for (size_t i = 0; i < offsets.size(); i++) {
        cur += offsets[i];
        if (i < offsets.size() - 1) {
            if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)cur, &cur, sizeof(cur), nullptr))
                return false;
        }
    }
    g_cameraStructAddr = cur;
    return true;
}

static bool WriteFloat(uintptr_t offset, float val) {
    if (!UpdateCameraAddress()) return false;
    uintptr_t target = g_cameraStructAddr + offset;
    DWORD oldProt;
    if (!VirtualProtect((LPVOID)target, sizeof(float), PAGE_EXECUTE_READWRITE, &oldProt)) return false;
    bool ok = WriteProcessMemory(GetCurrentProcess(), (LPVOID)target, &val, sizeof(val), nullptr) != 0;
    VirtualProtect((LPVOID)target, sizeof(float), oldProt, &oldProt);
    return ok;
}

static bool ReadFloatSafe(uintptr_t offset, float& value) {
    if (!UpdateCameraAddress()) return false;  
    uintptr_t target = g_cameraStructAddr + offset;
    return (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)target, &value, sizeof(float), nullptr) != 0);
}


static void ApplyCameraSettings() {
    
    if (!UpdateCameraAddress())
        return;

    WriteFloat(0x8, g_FOV);          
    WriteFloat(0x14, g_RenderMin);   
    WriteFloat(0x18, g_RenderMax);   
    WriteFloat(0x28, g_CameraSpeed); 

    static bool unknownsInitialized = false;
    static float unknowns[16]; 

    if (!unknownsInitialized) {
        uintptr_t offsets[] = { 0xC,0x10,0x1C,0x20,0x24,0x2C,0x30,0x34,0x38,0x3C,0x40,0x44,0x48,0x4C,0x50,0x54 };
        for (int i = 0; i < 16; i++) {
            ReadFloatSafe(offsets[i], unknowns[i]); 
        }
        unknownsInitialized = true;
    }
}


static void Worker() {
    std::this_thread::sleep_for(std::chrono::seconds(2));

    DWORD attr = GetFileAttributesA(GetConfigPath().c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        MessageBoxA(
            NULL,
            "Make sure to press the camera mode button (C) at least once to activate the mod.\n"
            "A config file will be created in the same directory!\n"
            "Default hotkeys are Right Control / Right Shift to adjust FOV",
            "The Crew FOV Mod Loaded (1st Time Only Message)",
            MB_OK | MB_ICONINFORMATION | MB_TOPMOST
        );
    }

    LoadConfig();
    HasConfigChanged();

    while (!Initialize())
        std::this_thread::sleep_for(std::chrono::seconds(1));

    int counter = 0;
    while (true) {

        if (HasConfigChanged()) {
            LoadConfig();
        }

        if (UpdateCameraAddress()) {
            ApplyCameraSettings();


            if (GetAsyncKeyState(g_IncreaseKey) & 0x8000) {
                g_FOV = std::clamp(g_FOV + g_Step, 0.0025f, 2.0f);
                SaveConfig();
            }
            if (GetAsyncKeyState(g_DecreaseKey) & 0x8000) {
                g_FOV = std::clamp(g_FOV - g_Step, 0.0025f, 2.0f);
                SaveConfig();
            }

        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}



static BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        std::thread(Worker).detach();
    }
    return TRUE;
}

