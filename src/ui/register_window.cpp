#include "register_window.h"
#include "documents_window.h"
#include "accounting_window.h"
#include "../data/document_store.h"
#include <sqlite3.h>
#include <shlobj.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <cwchar>

#pragma comment(lib, "comctl32.lib")

namespace {
struct Database {
    sqlite3* db = nullptr;
    ~Database() { if (db) sqlite3_close(db); }
};
struct Statement {
    sqlite3_stmt* stmt = nullptr;
    ~Statement() { if (stmt) sqlite3_finalize(stmt); }
};
struct RegisterState {
    Database database;
    HWND table = nullptr;
    HWND search = nullptr;
    HWND from = nullptr;
    HWND to = nullptr;
    HWND status = nullptr;
    HFONT font = nullptr;
    int count = 0;
    int companyId=0;
    std::wstring companyName;
    std::vector<sqlite3_int64> ids;
    std::vector<bool> workspace;
};

std::wstring GetText(HWND control) {
    int len = GetWindowTextLengthW(control);
    std::vector<wchar_t> text(static_cast<size_t>(len) + 1);
    GetWindowTextW(control, text.data(), len + 1);
    return text.data();
}
std::wstring ReadText(sqlite3_stmt* stmt, int column) {
    const void* value = sqlite3_column_text16(stmt, column);
    return value ? static_cast<const wchar_t*>(value) : L"";
}
std::wstring Money(sqlite3_int64 cents) {
    bool negative = cents < 0;
    if (negative) cents = -cents;
    return (negative ? L"-" : L"") + std::to_wstring(cents / 100) +
        L"." + (cents % 100 < 10 ? L"0" : L"") + std::to_wstring(cents % 100);
}
HWND Control(HWND parent, RegisterState* state, const wchar_t* cls,
             const wchar_t* title, DWORD style, int x, int y, int width, int height, int id = 0) {
    HWND control = CreateWindowExW(0, cls, title, WS_CHILD | WS_VISIBLE | style,
        x, y, width, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(state->font), TRUE);
    return control;
}
void Column(HWND list, int index, const wchar_t* title, int width) {
    LVCOLUMNW column = {};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.pszText = const_cast<LPWSTR>(title);
    column.cx = width;
    column.iSubItem = index;
    ListView_InsertColumn(list, index, &column);
}
bool OpenCompanyDatabase(Database& database, int companyId) {
    if (companyId <= 0) return false;
    wchar_t folder[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr,
                               SHGFP_TYPE_CURRENT, folder))) return false;
    std::wstring path = std::wstring(folder) + L"\\AME\\companies\\company_" +
                        std::to_wstring(companyId) + L".db";
    // Read-only: merely viewing the register must not create a database.
    sqlite3* handle = nullptr;
    int rc = sqlite3_open16(path.c_str(), &handle);
    if (rc != SQLITE_OK) {
        if (handle) sqlite3_close(handle);
        return false;
    }
    database.db = handle;
    sqlite3_busy_timeout(handle, 3000);
    return true;
}
void Refresh(HWND hwnd, RegisterState* state) {
    ListView_DeleteAllItems(state->table);
    state->count = 0;
    state->ids.clear();state->workspace.clear();
    bool hasInvoices = false;
    bool hasPayments = false;
    {
        Statement tables;
        if (sqlite3_prepare_v2(state->database.db,
            "SELECT name FROM sqlite_master WHERE type='table' AND name IN ('invoices','payments')",
            -1, &tables.stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(tables.stmt) == SQLITE_ROW) {
                std::wstring name = ReadText(tables.stmt, 0);
                if (name == L"invoices") hasInvoices = true;
                if (name == L"payments") hasPayments = true;
            }
        }
    }
    std::string kind = "CASE WHEN m.entry_id IS NOT NULL THEN "
        "CASE m.kind WHEN 1 THEN 'purchase' WHEN 2 THEN 'sale' ELSE 'free' END ";
    if (hasInvoices) kind += "WHEN EXISTS(SELECT 1 FROM invoices i WHERE i.entry=e.id) THEN 'invoice' ";
    if (hasPayments) kind += "WHEN EXISTS(SELECT 1 FROM payments p WHERE p.entry=e.id) THEN 'payment' ";
    kind += "ELSE 'journal' END ";
    std::string sql =
        "SELECT e.id, e.date, e.document, e.description, "
        "COALESCE(SUM(l.debit),0), " + kind + ", COALESCE(p.name,'') "
        "FROM journal_entries e "
        "LEFT JOIN journal_lines l ON l.entry_id=e.id "
        "LEFT JOIN am_documents m ON m.entry_id=e.id "
        "LEFT JOIN partners p ON p.id=m.partner_id "
        "WHERE (?1='' OR e.date>=?1) AND (?2='' OR e.date<=?2) "
        "AND (?3='' OR instr(lower(e.document),lower(?3))>0 "
        "OR instr(lower(e.description),lower(?3))>0 "
        "OR instr(lower(p.name),lower(?3))>0) "
        "GROUP BY e.id ORDER BY e.date DESC,e.id DESC";
    Statement query;
    if (sqlite3_prepare_v2(state->database.db, sql.c_str(), -1, &query.stmt, nullptr) != SQLITE_OK) {
        MessageBoxW(hwnd, L"Не може да се прочете описът. Проверете дали фирмата има счетоводен журнал.",
                    L"AME — Опис", MB_OK | MB_ICONWARNING);
        return;
    }
    const std::wstring from = GetText(state->from);
    const std::wstring to = GetText(state->to);
    const std::wstring search = GetText(state->search);
    sqlite3_bind_text16(query.stmt, 1, from.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text16(query.stmt, 2, to.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text16(query.stmt, 3, search.c_str(), -1, SQLITE_TRANSIENT);
    int rc = SQLITE_OK;
    while ((rc = sqlite3_step(query.stmt)) == SQLITE_ROW) {
        int index = state->count++;
        state->ids.push_back(sqlite3_column_int64(query.stmt,0));
        std::wstring id = std::to_wstring(sqlite3_column_int64(query.stmt, 0));
        std::wstring date = ReadText(query.stmt, 1);
        std::wstring number = ReadText(query.stmt, 2);
        std::wstring description = ReadText(query.stmt, 3);
        std::wstring amount = Money(sqlite3_column_int64(query.stmt, 4));
        std::wstring kind = ReadText(query.stmt, 5);
        bool modern=kind==L"purchase"||kind==L"sale"||kind==L"free";
        state->workspace.push_back(modern);
        if(kind==L"purchase")kind=L"Покупка";
        else if(kind==L"sale")kind=L"Продажба";
        else if(kind==L"free")kind=L"Свободна операция";
        else if (kind == L"invoice") kind = L"Фактура (стара)";
        else if (kind == L"payment") kind = L"Плащане";
        else kind = L"Операция";
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = index;
        item.pszText = const_cast<LPWSTR>(id.c_str());
        ListView_InsertItem(state->table, &item);
        std::wstring partner=ReadText(query.stmt,6);
        std::wstring values[] = {date, kind, number, partner,description, amount};
        for (int i = 0; i < 6; ++i)
            ListView_SetItemText(state->table, index, i + 1,
                                 const_cast<LPWSTR>(values[i].c_str()));
    }
    if (rc != SQLITE_DONE)
        MessageBoxW(hwnd, L"Грешка при четене на счетоводните документи.", L"AME", MB_OK | MB_ICONERROR);
    std::wstring status = L"Документи: " + std::to_wstring(state->count) +
        L"  |  Двоен клик или Отвори. Новите документи се редактират през Документи.";
    SetWindowTextW(state->status, status.c_str());
}
LRESULT CALLBACK RegisterProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    RegisterState* state = reinterpret_cast<RegisterState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        state = static_cast<RegisterState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (!state) return DefWindowProcW(hwnd, message, wParam, lParam);
    switch (message) {
    case WM_CREATE: {
        state->font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH, L"Tahoma");
        Control(hwnd, state, L"STATIC", L"От дата (ГГГГ-ММ-ДД)", 0, 18, 15, 185, 22);
        state->from = Control(hwnd, state, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                              18, 40, 170, 26);
        Control(hwnd, state, L"STATIC", L"До дата (ГГГГ-ММ-ДД)", 0, 205, 15, 185, 22);
        state->to = Control(hwnd, state, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                            205, 40, 170, 26);
        Control(hwnd, state, L"STATIC", L"Търси документ / описание", 0, 395, 15, 280, 22);
        state->search = Control(hwnd, state, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                                395, 40, 250, 26);
        Control(hwnd, state, L"BUTTON", L"Покажи", WS_TABSTOP | BS_PUSHBUTTON,
                660, 38, 115, 30, 1001);
        state->table = Control(hwnd, state, WC_LISTVIEWW, L"",
            WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            18, 83, 1015, 440, 1002);
        ListView_SetExtendedListViewStyle(state->table, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        Column(state->table, 0, L"ID", 50);
        Column(state->table, 1, L"Дата", 105);
        Column(state->table, 2, L"Вид", 142);
        Column(state->table, 3, L"Документ №", 128);
        Column(state->table, 4, L"Контрагент", 170);
        Column(state->table, 5, L"Описание", 290);
        Column(state->table, 6, L"Сума", 110);
        Control(hwnd, state, L"BUTTON", L"Отвори избрания",WS_TABSTOP|BS_PUSHBUTTON,18,540,170,32,1003);
        Control(hwnd, state, L"BUTTON", L"Нов документ",WS_TABSTOP|BS_PUSHBUTTON,200,540,170,32,1004);
        Control(hwnd, state, L"BUTTON", L"Затвори",WS_TABSTOP|BS_PUSHBUTTON,904,540,128,32,1005);
        state->status = Control(hwnd, state, L"STATIC", L"", 0, 18, 579, 1015, 24);
        Refresh(hwnd, state);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == 1001) { Refresh(hwnd, state); return 0; }
        if (LOWORD(wParam) == 1003) {
            int ix=ListView_GetNextItem(state->table,-1,LVNI_SELECTED);
            if(ix<0 || ix>=(int)state->ids.size()){
                MessageBoxW(hwnd,L"Първо изберете документ.",L"AME",MB_OK|MB_ICONINFORMATION);return 0;
            }
            auto entry=state->ids[(size_t)ix];
            if(state->workspace[(size_t)ix])
                ShowDocumentsWindow(hwnd,state->companyId,state->companyName,entry);
            else ShowJournalWindow(hwnd,state->companyId,state->companyName,entry);
            Refresh(hwnd,state);return 0;
        }
        if (LOWORD(wParam) == 1004) {ShowDocumentsWindow(hwnd,state->companyId,state->companyName);Refresh(hwnd,state);return 0;}
        if (LOWORD(wParam) == 1005) {SendMessageW(hwnd,WM_CLOSE,0,0);return 0;}
        if (HIWORD(wParam) == EN_CHANGE &&
            (reinterpret_cast<HWND>(lParam) == state->search)) {
            Refresh(hwnd, state); return 0;
        }
        return 0;
    case WM_NOTIFY:
        if(((NMHDR*)lParam)->hwndFrom==state->table && ((NMHDR*)lParam)->code==NM_DBLCLK){
            SendMessageW(hwnd,WM_COMMAND,1003,0);return 0;
        }
        return 0;
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        if (state->font) { DeleteObject(state->font); state->font = nullptr; }
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
}

void ShowRegisterWindow(HWND parent, int companyId, const std::wstring& companyName) {
    RegisterState state;
    state.companyId=companyId;state.companyName=companyName;
    if (!OpenCompanyDatabase(state.database, companyId)) {
        MessageBoxW(parent, L"Не може да се отвори базата на фирмата.", L"AME", MB_OK | MB_ICONERROR);
        return;
    }
    std::string schemaError;
    if (!ame_doc::EnsureSchema(state.database.db,schemaError)) {
        MessageBoxW(parent,L"Не може да се отвори регистърът на документите.",L"AME",MB_OK|MB_ICONERROR);return;
    }
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&controls);
    WNDCLASSW wc = {};
    wc.lpfnWndProc = RegisterProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"AME_DocumentRegister_020";
    RegisterClassW(&wc);
    std::wstring title = L"AME — Опис — " + companyName;
    HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME, wc.lpszClassName, title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
        1060, 650, parent, nullptr, wc.hInstance, &state);
    if (!window) return;
    EnableWindow(parent, FALSE);
    ShowWindow(window, SW_SHOW);
    MSG message;
    while (IsWindow(window)) {
        BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result <= 0) {
            if (result == 0) PostQuitMessage(static_cast<int>(message.wParam));
            break;
        }
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (IsWindow(window)) DestroyWindow(window);
    EnableWindow(parent, TRUE);
    SetActiveWindow(parent);
}
