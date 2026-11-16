#pragma once

#include <sqlite3.h>

class Database
{
public:
    Database();
    ~Database();

    bool Open(const char* path);
    void Close();

    bool Execute(const char* sql);

    sqlite3* GetHandle();

private:
    sqlite3* db;
};