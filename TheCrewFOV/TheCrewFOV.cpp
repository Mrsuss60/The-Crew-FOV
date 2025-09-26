#include "pch.h"


float g_FOV = 1.30f;
const float g_RenderMin = 0.01f;
const float g_RenderMax = 20000.0f;
const float g_CameraSpeed = 115.0f;

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


static std::string GetConfigPath() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string path = exePath;
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos) path = path.substr(0, pos + 1);
    return path + "TheCrewFOV.cfg";
}

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


static void SaveConfig() {
    std::ofstream f(GetConfigPath());
    if (!f.is_open()) return;
    f << "# FOV can be set from 0.025 to 2.0\n";  
    f << "FOV=" << g_FOV << "\n";
}

static void LoadConfig() {
    std::ifstream f(GetConfigPath());
    if (!f.is_open()) { SaveConfig(); return; }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '|') continue;  
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        float val = std::stof(line.substr(eq + 1));
        if (key == "FOV") {
            g_FOV = std::clamp(val, 0.025f, 2.0f);
        }
    }
}

static void Worker() {
    std::this_thread::sleep_for(std::chrono::seconds(3));
    while (!Initialize()) std::this_thread::sleep_for(std::chrono::seconds(1));
    LoadConfig();

    int counter = 0;
    while (true) {
        if (UpdateCameraAddress()) {   
            ApplyCameraSettings();     
            if ((GetAsyncKeyState(VK_CONTROL) & 0x8000)) { 
                if (GetAsyncKeyState(VK_ADD) & 0x8000) {
                    g_FOV += 0.025f;
                    if (g_FOV > 2.0f) g_FOV = 2.0f;
                    SaveConfig();
                }
                if (GetAsyncKeyState(VK_SUBTRACT) & 0x8000) {
                    g_FOV -= 0.025f;
                    if (g_FOV < 0.025f) g_FOV = 0.025f;
                    SaveConfig();
                }
            }

            if (GetAsyncKeyState(VK_RSHIFT) & 0x8000) {
                g_FOV += 0.025f;
                if (g_FOV > 2.0f) g_FOV = 2.0f;
                SaveConfig();
            }
            if (GetAsyncKeyState(VK_RCONTROL) & 0x8000) {
                g_FOV -= 0.025f;
                if (g_FOV < 0.025f) g_FOV = 0.025f;
                SaveConfig();
            }

            if (++counter >= 50) {
                LoadConfig();
                counter = 0;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}


static BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        
        DWORD attr = GetFileAttributesA(GetConfigPath().c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) {
            MessageBoxA(
                NULL,
                "Make sure to press the camera mode button (C) at least once to activate the mod.\nA config file will be created in the same directory!\nCtrl + Numpad+/- to adjust the FOV\nOr Right Control / Right shift keys",
                "The Crew FOV Mod Loaded (1st Time Only Message)",
                MB_OK | MB_ICONINFORMATION | MB_TOPMOST
            );
        }

        LoadConfig();


        std::thread(Worker).detach();
    }
    return TRUE;
}

