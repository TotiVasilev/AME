#pragma once
#include <windows.h>
#include <string>

// Real working additions to the original AME application. Every module uses
// the active company's existing SQLite database; no data is deleted.
void ShowAMEReport(HWND parent, int companyId, const std::wstring& companyName, int reportIndex);
void ShowAMEPeriod(HWND parent, int companyId, const std::wstring& companyName);
// type: 0 counterparties, 1 expense nomenclature, 2 employees,
//       3 fixed assets, 4 material nomenclature, 5 warehouses.
void ShowAMEDirectory(HWND parent, int companyId, const std::wstring& companyName, int type);
