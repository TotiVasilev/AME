#pragma once
#include <windows.h>
#include <string>
#include "../data/company.h"
bool RememberActiveCompany(int id);
bool RestoreActiveCompany(Company& company);
void ShowAccountingWindow(HWND parent, int id, const std::wstring& name, bool opening);
