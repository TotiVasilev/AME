#pragma once
#include <windows.h>
#include <string>

// Read-only register of posted accounting documents for the active company.
void ShowRegisterWindow(HWND parent, int companyId, const std::wstring& companyName);
