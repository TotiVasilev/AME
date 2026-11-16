#include "company.h"
#include <windows.h>
#include <shlobj.h>
#include <sqlite3.h>

static std::wstring lastError;

static std::wstring ReadText(
    sqlite3_stmt* stmt,
    int column
)
{
    const void* text =
        sqlite3_column_text16(stmt, column);

    if (!text)
        return L"";

    return static_cast<const wchar_t*>(text);
}

static void SetError(sqlite3* db)
{
    lastError = static_cast<const wchar_t*>(
        sqlite3_errmsg16(db)
    );
}

std::wstring GetCompanyError()
{
    return lastError;
}

static bool OpenDatabase(sqlite3** db)
{
    wchar_t folder[MAX_PATH];

    HRESULT result = SHGetFolderPathW(
        NULL,
        CSIDL_LOCAL_APPDATA |
        CSIDL_FLAG_CREATE,
        NULL,
        SHGFP_TYPE_CURRENT,
        folder
    );

    if (FAILED(result))
    {
        lastError =
            L"Не може да се намери папката за данни.";

        return false;
    }

    std::wstring path = folder;
    path += L"\\AME";

    if (!CreateDirectoryW(path.c_str(), NULL))
    {
        if (GetLastError() != ERROR_ALREADY_EXISTS)
        {
            lastError =
                L"Не може да се създаде папката AME.";

            return false;
        }
    }

    path += L"\\companies.db";

    int rc = sqlite3_open16(
        path.c_str(),
        db
    );

    if (rc != SQLITE_OK)
    {
        if (*db)
        {
            SetError(*db);
            sqlite3_close(*db);
            *db = nullptr;
        }
        else
        {
            lastError =
                L"Грешка при отваряне на базата.";
        }

        return false;
    }

    sqlite3_busy_timeout(*db, 3000);

    return true;
}

bool InitializeCompanies()
{
    sqlite3* db = nullptr;

    if (!OpenDatabase(&db))
        return false;

    const char* sql =
        "CREATE TABLE IF NOT EXISTS companies ("
        "id INTEGER PRIMARY KEY,"
        "name TEXT NOT NULL,"
        "eik TEXT NOT NULL UNIQUE,"
        "vat_number TEXT,"
        "manager TEXT,"
        "address TEXT,"
        "city TEXT,"
        "phone TEXT,"
        "email TEXT"
        ");";

    int rc = sqlite3_exec(
        db,
        sql,
        nullptr,
        nullptr,
        nullptr
    );

    if (rc != SQLITE_OK)
        SetError(db);

    sqlite3_close(db);

    return rc == SQLITE_OK;
}

bool SaveCompany(Company& company)
{
    sqlite3* db = nullptr;

    if (!OpenDatabase(&db))
        return false;

    const char* sql;

    if (company.id == 0)
    {
        sql =
            "INSERT INTO companies "
            "(name, eik, vat_number, manager, "
            "address, city, phone, email) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    }
    else
    {
        sql =
            "UPDATE companies SET "
            "name=?, eik=?, vat_number=?, "
            "manager=?, address=?, city=?, "
            "phone=?, email=? WHERE id=?;";
    }

    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &stmt,
        nullptr
    );

    if (rc != SQLITE_OK)
    {
        SetError(db);
        sqlite3_close(db);
        return false;
    }

    const std::wstring* fields[] = {
        &company.name,
        &company.eik,
        &company.vatNumber,
        &company.manager,
        &company.address,
        &company.city,
        &company.phone,
        &company.email
    };

    for (int i = 0; i < 8; i++)
    {
        rc = sqlite3_bind_text16(
            stmt,
            i + 1,
            fields[i]->c_str(),
            -1,
            SQLITE_TRANSIENT
        );

        if (rc != SQLITE_OK)
            break;
    }

    if (rc == SQLITE_OK && company.id != 0)
    {
        rc = sqlite3_bind_int(
            stmt,
            9,
            company.id
        );
    }

    if (rc == SQLITE_OK)
        rc = sqlite3_step(stmt);

    bool success = (rc == SQLITE_DONE);

    if (success)
    {
        if (company.id == 0)
        {
            company.id = static_cast<int>(
                sqlite3_last_insert_rowid(db)
            );
        }
    }
    else
    {
        SetError(db);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return success;
}

bool LoadCompanies(
    std::vector<Company>& companies
)
{
    companies.clear();

    sqlite3* db = nullptr;

    if (!OpenDatabase(&db))
        return false;

    const char* sql =
        "SELECT id, name, eik, vat_number, "
        "manager, address, city, phone, email "
        "FROM companies ORDER BY name;";

    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(
        db,
        sql,
        -1,
        &stmt,
        nullptr
    );

    if (rc != SQLITE_OK)
    {
        SetError(db);
        sqlite3_close(db);
        return false;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        Company company;

        company.id = sqlite3_column_int(stmt, 0);
        company.name = ReadText(stmt, 1);
        company.eik = ReadText(stmt, 2);
        company.vatNumber = ReadText(stmt, 3);
        company.manager = ReadText(stmt, 4);
        company.address = ReadText(stmt, 5);
        company.city = ReadText(stmt, 6);
        company.phone = ReadText(stmt, 7);
        company.email = ReadText(stmt, 8);

        companies.push_back(company);
    }

    bool success = (rc == SQLITE_DONE);

    if (!success)
        SetError(db);

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return success;
}