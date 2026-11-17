#include "chart_template.h"
#include <windows.h>
#include <shlobj.h>
#include <sqlite3.h>
#include <string>

namespace {
struct AccountSeed {
    const wchar_t* code;
    const wchar_t* name;
    int kind;   // 0 asset, 1 liability, 2 equity, 3 revenue, 4 expense
};

// Editable starter template for ordinary Bulgarian businesses.
// This is NOT a mandatory national chart or the НАП SAF-T reference mapping.
// Account codes used by a business must be approved against its own accounting policy.
const AccountSeed kTemplate[] = {
    {L"101", L"Основен капитал", 2},
    {L"102", L"Допълнителен капитал", 2},
    {L"103", L"Капитал при персонални дружества", 2},
    {L"104", L"Капитал на предприятия с нестопанска цел", 2},
    {L"111", L"Общи резерви", 2},
    {L"112", L"Резерв от последващи оценки", 2},
    {L"113", L"Резерв от емисии на акции", 2},
    {L"117", L"Други резерви", 2},
    {L"121", L"Непокрита загуба от минали години", 2},
    {L"122", L"Неразпределена печалба от минали години", 2},
    {L"123", L"Печалби и загуби от текущата година", 2},
    {L"124", L"Резултат при ликвидация и несъстоятелност", 2},
    {L"131", L"Приходи за бъдещи периоди", 1},
    {L"151", L"Получени краткосрочни заеми", 1},
    {L"152", L"Получени дългосрочни заеми", 1},
    {L"153", L"Получени заеми от свързани предприятия", 1},
    {L"159", L"Други заеми и дългове", 1},
    {L"161", L"Отсрочени данъчни пасиви", 1},
    {L"201", L"Земи, гори и трайни насаждения", 0},
    {L"202", L"Подобрения върху наети активи", 0},
    {L"203", L"Сгради", 0},
    {L"204", L"Машини, съоръжения и оборудване", 0},
    {L"205", L"Транспортни средства", 0},
    {L"206", L"Стопански инвентар", 0},
    {L"207", L"Разходи за придобиване на ДМА", 0},
    {L"208", L"Ликвидация на ДМА", 0},
    {L"209", L"Други дълготрайни материални активи", 0},
    {L"211", L"Продукти от развойна дейност", 0},
    {L"212", L"Концесии", 0},
    {L"213", L"Патенти, лицензи и търговски марки", 0},
    {L"214", L"Програмни продукти", 0},
    {L"219", L"Други дълготрайни нематериални активи", 0},
    {L"221", L"Дългосрочни инвестиции в дъщерни предприятия", 0},
    {L"222", L"Дългосрочни инвестиции в асоциирани предприятия", 0},
    {L"223", L"Други дългосрочни финансови активи", 0},
    {L"241", L"Натрупана амортизация на ДМА", 0},
    {L"242", L"Натрупана амортизация на НДА", 0},
    {L"252", L"Обезценка на вземания", 0},
    {L"301", L"Доставки", 0},
    {L"302", L"Материали", 0},
    {L"303", L"Продукция", 0},
    {L"304", L"Стоки", 0},
    {L"305", L"Амбалаж", 0},
    {L"306", L"Млади животни и животни за угояване", 0},
    {L"307", L"Незавършено производство", 0},
    {L"308", L"Резервни части", 0},
    {L"309", L"Други материални запаси", 0},
    {L"401", L"Доставчици", 1},
    {L"402", L"Аванси към доставчици", 0},
    {L"403", L"Задължения към доставчици по търговски кредити", 1},
    {L"404", L"Задължения към доставчици при определени условия", 1},
    {L"405", L"Задължения към свързани доставчици", 1},
    {L"409", L"Други задължения към доставчици", 1},
    {L"411", L"Клиенти", 0},
    {L"412", L"Аванси от клиенти", 1},
    {L"413", L"Вземания от клиенти по търговски кредити", 0},
    {L"414", L"Вземания от клиенти при определени условия", 0},
    {L"419", L"Други вземания от клиенти", 0},
    {L"421", L"Персонал", 1},
    {L"422", L"Подотчетни лица", 0},
    {L"423", L"Задължения по неизползвани отпуски", 1},
    {L"424", L"Разчети по участия в печалбата", 1},
    {L"426", L"Вземания по записани дялови вноски", 0},
    {L"429", L"Други разчети с персонала", 1},
    {L"441", L"Вземания по рекламации", 0},
    {L"442", L"Вземания по липси и начети", 0},
    {L"443", L"Ценови разлики по липси и начети", 0},
    {L"444", L"Вземания по съдебни спорове", 0},
    {L"445", L"Присъдени вземания", 0},
    {L"451", L"Разчети с общините", 1},
    {L"452", L"Разчети за корпоративен данък", 1},
    {L"4531", L"ДДС по покупки", 0},
    {L"4532", L"ДДС по продажби", 1},
    {L"4538", L"ДДС за възстановяване", 0},
    {L"4539", L"ДДС за внасяне", 1},
    {L"454", L"Разчети за данъци върху доходите на физически лица", 1},
    {L"455", L"Разчети по социално осигуряване", 1},
    {L"456", L"Разчети по здравно осигуряване", 1},
    {L"457", L"Разчети с митници", 1},
    {L"458", L"Разчети за други данъци и такси", 1},
    {L"459", L"Други разчети с бюджета", 1},
    {L"461", L"Разчети със свързани предприятия", 0},
    {L"462", L"Разчети по дивиденти", 1},
    {L"471", L"Разчети по гаранции", 1},
    {L"481", L"Разходи за бъдещи периоди", 0},
    {L"482", L"Приходи за бъдещи периоди", 1},
    {L"491", L"Доверители", 1},
    {L"492", L"Довереници", 0},
    {L"496", L"Разчети по лихви", 1},
    {L"497", L"Разчети по данъчни временни разлики", 0},
    {L"498", L"Други дебитори", 0},
    {L"499", L"Други кредитори", 1},
    {L"501", L"Каса", 0},
    {L"502", L"Каса във валута", 0},
    {L"503", L"Разплащателна сметка", 0},
    {L"504", L"Разплащателна сметка във валута", 0},
    {L"505", L"Акредитиви", 0},
    {L"506", L"Предоставени парични депозити", 0},
    {L"507", L"Парични средства в движение", 0},
    {L"508", L"Краткосрочни финансови активи", 0},
    {L"509", L"Други парични средства", 0},
    {L"601", L"Разходи за материали", 4},
    {L"602", L"Разходи за външни услуги", 4},
    {L"603", L"Разходи за амортизации", 4},
    {L"604", L"Разходи за заплати", 4},
    {L"605", L"Разходи за осигуровки", 4},
    {L"606", L"Разходи за данъци, такси и подобни плащания", 4},
    {L"607", L"Разходи за провизии", 4},
    {L"608", L"Разходи от последващи оценки на активи", 4},
    {L"609", L"Други разходи", 4},
    {L"611", L"Разходи за основна дейност", 4},
    {L"612", L"Разходи за спомагателна дейност", 4},
    {L"613", L"Разходи за бъдещи периоди", 0},
    {L"614", L"Разходи за организация и управление", 4},
    {L"615", L"Разходи за продажби", 4},
    {L"616", L"Разходи за придобиване на активи по стопански начин", 4},
    {L"621", L"Разходи за лихви", 4},
    {L"622", L"Разходи от операции с финансови активи", 4},
    {L"623", L"Разходи от обезценка на финансови активи", 4},
    {L"624", L"Разходи по валутни операции", 4},
    {L"625", L"Финансови разходи за бъдещи периоди", 0},
    {L"629", L"Други финансови разходи", 4},
    {L"691", L"Отписани вземания", 4},
    {L"692", L"Разходи за глоби и неустойки", 4},
    {L"693", L"Липси на активи", 4},
    {L"699", L"Други извънредни разходи", 4},
    {L"701", L"Приходи от продажба на продукция", 3},
    {L"702", L"Приходи от продажба на стоки", 3},
    {L"703", L"Приходи от продажба на услуги", 3},
    {L"704", L"Приходи от валутни операции", 3},
    {L"705", L"Приходи от продажба на дълготрайни активи", 3},
    {L"706", L"Приходи от продажба на материали", 3},
    {L"707", L"Приходи от финансирания", 3},
    {L"709", L"Други приходи от дейността", 3},
    {L"721", L"Приходи от лихви", 3},
    {L"722", L"Приходи от участия", 3},
    {L"723", L"Приходи от операции с финансови активи", 3},
    {L"724", L"Приходи от валутни операции", 3},
    {L"725", L"Финансови приходи за бъдещи периоди", 1},
    {L"729", L"Други финансови приходи", 3},
    {L"791", L"Отписани задължения", 3},
    {L"792", L"Приходи от глоби и неустойки", 3},
    {L"793", L"Излишъци на активи", 3},
    {L"799", L"Други извънредни приходи", 3},
};

bool exec(sqlite3* db, const char* query) {
    return sqlite3_exec(db, query, nullptr, nullptr, nullptr) == SQLITE_OK;
}

void storeError(sqlite3* db, std::wstring* error) {
    if (!error) return;
    const void* msg = db ? sqlite3_errmsg16(db) : nullptr;
    *error = msg ? static_cast<const wchar_t*>(msg) : L"Грешка при отваряне на SQLite.";
}

bool schema(sqlite3* db) {
    return exec(db,
        "PRAGMA foreign_keys=ON;"
        "CREATE TABLE IF NOT EXISTS accounts("
        "id INTEGER PRIMARY KEY, code TEXT NOT NULL UNIQUE, name TEXT NOT NULL,"
        "kind INTEGER NOT NULL CHECK(kind BETWEEN 0 AND 4),"
        "active INTEGER NOT NULL CHECK(active IN (0,1)));"
        "CREATE TABLE IF NOT EXISTS opening("
        "account_id INTEGER NOT NULL REFERENCES accounts(id),"
        "date TEXT NOT NULL, debit INTEGER NOT NULL CHECK(debit>=0),"
        "credit INTEGER NOT NULL CHECK(credit>=0),"
        "PRIMARY KEY(account_id,date), CHECK(debit=0 OR credit=0));"
        "CREATE TABLE IF NOT EXISTS ame_chart_template_meta("
        "key TEXT PRIMARY KEY, value INTEGER NOT NULL);");
}

bool apply(sqlite3* db, bool force, int* inserted, std::wstring* error) {
    if (inserted) *inserted = 0;
    if (error) error->clear();
    if (!db) { storeError(nullptr, error); return false; }
    if (!schema(db)) { storeError(db,error); return false; }

    if (!force) {
        sqlite3_stmt* check = nullptr;
        int rc = sqlite3_prepare_v2(db,
            "SELECT value FROM ame_chart_template_meta WHERE key='version'",
            -1, &check, nullptr);
        if (rc != SQLITE_OK) { storeError(db,error); return false; }
        rc = sqlite3_step(check);
        bool already = (rc == SQLITE_ROW && sqlite3_column_int(check,0) >= 1);
        bool checked = (rc == SQLITE_DONE || rc == SQLITE_ROW);
        sqlite3_finalize(check);
        if (!checked) { storeError(db,error); return false; }
        if (already) return true;
    }

    if (!exec(db,"BEGIN IMMEDIATE")) { storeError(db,error); return false; }
    sqlite3_stmt* add = nullptr;
    bool ok = sqlite3_prepare_v2(db,
        "INSERT OR IGNORE INTO accounts(code,name,kind,active) VALUES(?,?,?,1)",
        -1, &add, nullptr) == SQLITE_OK;
    int count = 0;
    for (const AccountSeed& a : kTemplate) {
        if (!ok) break;
        ok = sqlite3_bind_text16(add,1,a.code,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
             sqlite3_bind_text16(add,2,a.name,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
             sqlite3_bind_int(add,3,a.kind)==SQLITE_OK &&
             sqlite3_step(add)==SQLITE_DONE;
        if (ok) count += sqlite3_changes(db);
        if (ok) ok = sqlite3_reset(add)==SQLITE_OK &&
                     sqlite3_clear_bindings(add)==SQLITE_OK;
    }
    if (add) sqlite3_finalize(add);
    if (ok) ok = exec(db,
        "INSERT OR REPLACE INTO ame_chart_template_meta(key,value) VALUES('version',1)");
    if (ok) ok = exec(db,"COMMIT");
    if (!ok) {
        storeError(db,error);
        exec(db,"ROLLBACK");
        return false;
    }
    if (inserted) *inserted = count;
    return true;
}
}

int TemplateAccountCount() {
    return static_cast<int>(sizeof(kTemplate)/sizeof(kTemplate[0]));
}

bool EnsureChartTemplate(sqlite3* db, int* inserted, std::wstring* error) {
    return apply(db,false,inserted,error);
}

bool AddMissingTemplateAccounts(sqlite3* db, int* inserted, std::wstring* error) {
    return apply(db,true,inserted,error);
}

bool InitializeCompanyChart(int companyId, std::wstring* error) {
    if (error) error->clear();
    if (companyId <= 0) { if (error) *error=L"Невалиден номер на фирма."; return false; }
    wchar_t folder[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE,
                                nullptr,SHGFP_TYPE_CURRENT,folder))) {
        if (error) *error=L"Не може да се намери папката с данни.";
        return false;
    }
    std::wstring path=std::wstring(folder)+L"\\AME";
    if (!CreateDirectoryW(path.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS) {
        if (error) *error=L"Не може да се създаде папката AME.";
        return false;
    }
    path+=L"\\companies";
    if (!CreateDirectoryW(path.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS) {
        if (error) *error=L"Не може да се създаде папката на фирмите.";
        return false;
    }
    path+=L"\\company_"+std::to_wstring(companyId)+L".db";
    sqlite3* db = nullptr;
    if (sqlite3_open16(path.c_str(),&db)!=SQLITE_OK) {
        storeError(db,error);
        if (db) sqlite3_close(db);
        return false;
    }
    sqlite3_busy_timeout(db,3000);
    bool ok=EnsureChartTemplate(db,nullptr,error);
    sqlite3_close(db);
    return ok;
}
