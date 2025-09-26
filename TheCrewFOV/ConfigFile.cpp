#include "pch.h"

float g_FOV = 1.7f;
float g_Step = 0.0025f;
int g_IncreaseKey = VK_RSHIFT;
int g_DecreaseKey = VK_RCONTROL;


std::string GetConfigPath() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string path = exePath;
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos) path = path.substr(0, pos + 1);
    return path + "TheCrewFOV.cfg";
}

bool HasConfigChanged() {
    struct stat fileStat;
    if (stat(GetConfigPath().c_str(), &fileStat) == 0) {
        if (fileStat.st_mtime != g_lastConfigModTime) {
            g_lastConfigModTime = fileStat.st_mtime;
            return true;
        }
    }
    return false;
}

void SaveConfig() {
    std::ofstream f(GetConfigPath());
    if (!f.is_open()) return;
    f << "# FOV can be set from 0.2 to 2.5\n";
    f << "FOV=" << g_FOV << "\n";
    f << "# Stepping Factor (0.0025 - 0.1)\n";
    f << "Step=" << g_Step << "\n";
    f << "# VK keys (you can find them here https://github.com/sepehrsohrabi/Decimal-Virtual-Key-Codes)\n";
    f << "IncreaseKey=" << g_IncreaseKey << "\n";
    f << "DecreaseKey=" << g_DecreaseKey << "\n";
}

void LoadConfig() {
    std::ifstream f(GetConfigPath());
    if (!f.is_open()) { SaveConfig(); return; }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '|') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        try {
            if (key == "FOV") {
                g_FOV = std::clamp(std::stof(val), 0.2f, 2.5f);
            }
            else if (key == "Step") {
                g_Step = std::clamp(std::stof(val), 0.0025f, 0.1f);
            }
            else if (key == "IncreaseKey") {
                g_IncreaseKey = std::stoi(val);
            }
            else if (key == "DecreaseKey") {
                g_DecreaseKey = std::stoi(val);
            }
        }
        catch (...)
        {
        }
    }
}
