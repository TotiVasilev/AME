#pragma once
#include <windows.h>
#include <string>
// Detailed per-employee insurance workpaper. Verified, user-selected rates
// (not universal statutory presets). Stores individual fund components.
void ShowAMEFundCalculator(HWND parent,int companyId,const std::wstring& companyName);
