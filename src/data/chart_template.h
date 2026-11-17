#pragma once
#include <string>
struct sqlite3;

// Adds the built-in Bulgarian business chart on the first opening of a company.
// Existing codes (including their names, type, active flag and IDs) stay untouched.
bool EnsureChartTemplate(sqlite3* database, int* inserted = nullptr,
                         std::wstring* error = nullptr);

// Manually add missing template codes. This never changes existing accounts.
bool AddMissingTemplateAccounts(sqlite3* database, int* inserted = nullptr,
                                std::wstring* error = nullptr);

// Creates/initializes a company's accounting database immediately after registration.
bool InitializeCompanyChart(int companyId, std::wstring* error = nullptr);

int TemplateAccountCount();
