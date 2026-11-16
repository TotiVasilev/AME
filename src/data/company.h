#pragma once

#include <string>
#include <vector>

struct Company
{
    int id = 0;

    std::wstring name;
    std::wstring eik;
    std::wstring vatNumber;
    std::wstring manager;
    std::wstring address;
    std::wstring city;
    std::wstring phone;
    std::wstring email;
};

bool InitializeCompanies();

bool SaveCompany(Company& company);

bool LoadCompanies(
    std::vector<Company>& companies
);

std::wstring GetCompanyError();