#pragma once
#include <windows.h>
#include <string>

// Complementary accounting workbenches. Modes: 0 VAT review, 1 rate-based
// payroll estimate, 2 D1/D6 reconciliation, 3 ETZ internal XML, 4 tax
// depreciation schedule, 5 year-end transfer and permanent period lock.
// IMPORTANT: No modes produce NRA-submittable files.
void ShowAMECompliance(HWND parent,int companyId,const std::wstring& name,int mode);
