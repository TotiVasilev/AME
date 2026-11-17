#pragma once
#include <windows.h>
#include <string>

// 0: VAT register/workpaper, 1: payroll workpaper, 2: employment document
// register, 3: depreciation workpaper, 4: year-end diagnostic.
// These are internal records and drafts; NOT validated statutory filings.
void ShowAMEProfessional(HWND parent, int companyId, const std::wstring& companyName,
                         int mode, int suggestedType = 0);
