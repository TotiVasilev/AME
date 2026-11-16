#pragma once

#include <windows.h>
#include <string>
#include "../data/company.h"

bool ShowCompaniesWindow(
    HWND parent,
    Company& selectedCompany
);