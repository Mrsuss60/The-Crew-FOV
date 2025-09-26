#pragma once
#include "pch.h"

extern float g_FOV;
extern float g_Step;
extern int   g_IncreaseKey;
extern int   g_DecreaseKey;

static time_t g_lastConfigModTime = 0;

std::string GetConfigPath();

bool HasConfigChanged();

void SaveConfig();
void LoadConfig();
