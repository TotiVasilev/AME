#pragma once
#include <windows.h>
#include <string>
#include <cstdint>

// VIVA-familiar document entry: Free operations, Purchases, Sales.
// Saves connected journal entries and document metadata atomically.
// initialEntryId=0 creates a new document; otherwise opens an AME document.
void ShowDocumentsWindow(HWND parent,int companyId,const std::wstring& companyName,
                         std::int64_t initialEntryId=0);
