#include "database.h"

Database::Database()
    : db(nullptr)
{
}

Database::~Database()
{
    Close();
}

bool Database::Open(const char* path)
{
    Close();

    int result = sqlite3_open(path, &db);

    if (result != SQLITE_OK)
    {
        Close();
        return false;
    }

    return true;
}

void Database::Close()
{
    if (db)
    {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool Database::Execute(const char* sql)
{
    if (!db)
    {
        return false;
    }

    int result = sqlite3_exec(
        db,
        sql,
        nullptr,
        nullptr,
        nullptr
    );

    return result == SQLITE_OK;
}

sqlite3* Database::GetHandle()
{
    return db;
}