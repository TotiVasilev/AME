#include "companies_window.h"
#include "../data/company.h"

#include <vector>
#include <string>

#define ID_COMPANY_LIST  2001
#define ID_NEW_COMPANY   2002
#define ID_SAVE_COMPANY  2003
#define ID_SELECT_COMPANY 2004
#define ID_CLOSE_COMPANY 2005

struct CompanyWindowState
{
    HWND list;
    HWND fields[8];

    HFONT font;

    std::vector<Company> companies;

    int editingId = 0;

    Company selectedCompany;

    bool selected = false;
    bool closed = false;
};

static HWND AddControl(
    HWND parent,
    const wchar_t* className,
    const wchar_t* text,
    DWORD style,
    DWORD exStyle,
    int x,
    int y,
    int width,
    int height,
    int id,
    HFONT font
)
{
    HWND control = CreateWindowExW(
        exStyle,
        className,
        text,
        WS_CHILD | WS_VISIBLE | style,
        x, y, width, height,
        parent,
        (HMENU)(INT_PTR)id,
        GetModuleHandleW(NULL),
        NULL
    );

    SendMessageW(
        control,
        WM_SETFONT,
        (WPARAM)font,
        TRUE
    );

    return control;
}

static void ShowError(HWND hwnd)
{
    MessageBoxW(
        hwnd,
        GetCompanyError().c_str(),
        L"Грешка",
        MB_OK | MB_ICONERROR
    );
}

static void ClearFields(
    CompanyWindowState* state
)
{
    state->editingId = 0;

    for (int i = 0; i < 8; i++)
    {
        SetWindowTextW(
            state->fields[i],
            L""
        );
    }

    SendMessageW(
        state->list,
        LB_SETCURSEL,
        (WPARAM)-1,
        0
    );

    SetFocus(state->fields[0]);
}

static bool RefreshList(
    HWND hwnd,
    CompanyWindowState* state
)
{
    std::vector<Company> companies;

    if (!LoadCompanies(companies))
    {
        ShowError(hwnd);
        return false;
    }

    state->companies.swap(companies);

    SendMessageW(
        state->list,
        LB_RESETCONTENT,
        0, 0
    );

    for (const Company& company : state->companies)
    {
        std::wstring text =
            company.name +
            L" (" + company.eik + L")";

        SendMessageW(
            state->list,
            LB_ADDSTRING,
            0,
            (LPARAM)text.c_str()
        );
    }

    return true;
}

static void LoadSelected(
    CompanyWindowState* state
)
{
    int index = (int)SendMessageW(
        state->list,
        LB_GETCURSEL,
        0, 0
    );

    if (index < 0 ||
        index >= (int)state->companies.size())
    {
        return;
    }

    const Company& company =
        state->companies[index];

    state->editingId = company.id;

    const std::wstring* values[] = {
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
        SetWindowTextW(
            state->fields[i],
            values[i]->c_str()
        );
    }
}

static std::wstring GetFieldText(HWND field)
{
    wchar_t buffer[512];

    GetWindowTextW(
        field,
        buffer,
        512
    );

    std::wstring text = buffer;

    size_t first =
        text.find_first_not_of(L" \t\r\n");

    if (first == std::wstring::npos)
        return L"";

    size_t last =
        text.find_last_not_of(L" \t\r\n");

    return text.substr(
        first,
        last - first + 1
    );
}

static void SaveFields(
    HWND hwnd,
    CompanyWindowState* state
)
{
    Company company;

    company.id = state->editingId;

    company.name = GetFieldText(state->fields[0]);
    company.eik = GetFieldText(state->fields[1]);
    company.vatNumber = GetFieldText(state->fields[2]);
    company.manager = GetFieldText(state->fields[3]);
    company.address = GetFieldText(state->fields[4]);
    company.city = GetFieldText(state->fields[5]);
    company.phone = GetFieldText(state->fields[6]);
    company.email = GetFieldText(state->fields[7]);

    if (company.name.empty() ||
        company.eik.empty())
    {
        MessageBoxW(
            hwnd,
            L"Въведете наименование и ЕИК.",
            L"Внимание",
            MB_OK | MB_ICONWARNING
        );

        return;
    }

    if (company.eik.length() != 9 &&
        company.eik.length() != 13)
    {
        MessageBoxW(
            hwnd,
            L"ЕИК трябва да съдържа 9 или 13 цифри.",
            L"Внимание",
            MB_OK | MB_ICONWARNING
        );

        return;
    }

    for (wchar_t c : company.eik)
    {
        if (c < L'0' || c > L'9')
        {
            MessageBoxW(
                hwnd,
                L"ЕИК трябва да съдържа само цифри.",
                L"Внимание",
                MB_OK | MB_ICONWARNING
            );

            return;
        }
    }

    if (!SaveCompany(company))
    {
        ShowError(hwnd);
        return;
    }

    state->editingId = company.id;

    if (RefreshList(hwnd, state))
    {
        for (int i = 0;
             i < (int)state->companies.size();
             i++)
        {
            if (state->companies[i].id == company.id)
            {
                SendMessageW(
                    state->list,
                    LB_SETCURSEL,
                    i, 0
                );

                break;
            }
        }
    }

    MessageBoxW(
        hwnd,
        L"Фирмата е записана успешно.",
        L"AME",
        MB_OK | MB_ICONINFORMATION
    );
}

static LRESULT CALLBACK CompaniesProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
)
{
    CompanyWindowState* state =
        reinterpret_cast<CompanyWindowState*>(
            GetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA
            )
        );

    switch (msg)
    {
        case WM_NCCREATE:
        {
            CREATESTRUCTW* cs =
                reinterpret_cast<CREATESTRUCTW*>(
                    lParam
                );

            SetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA,
                (LONG_PTR)cs->lpCreateParams
            );

            return TRUE;
        }

        case WM_CREATE:
        {
            state->font = (HFONT)GetStockObject(
                DEFAULT_GUI_FONT
            );

            AddControl(
                hwnd, L"STATIC",
                L"Записани фирми",
                0, 0,
                15, 15, 250, 25,
                0, state->font
            );

            state->list = AddControl(
                hwnd, L"LISTBOX", L"",
                WS_TABSTOP |
                WS_VSCROLL |
                LBS_NOTIFY |
                LBS_NOINTEGRALHEIGHT,
                WS_EX_CLIENTEDGE,
                15, 45, 260, 315,
                ID_COMPANY_LIST,
                state->font
            );

            const wchar_t* labels[] = {
                L"Наименование:",
                L"ЕИК / БУЛСТАТ:",
                L"ДДС номер:",
                L"МОЛ:",
                L"Адрес:",
                L"Град:",
                L"Телефон:",
                L"Имейл:"
            };

            for (int i = 0; i < 8; i++)
            {
                int y = 20 + i * 42;

                AddControl(
                    hwnd,
                    L"STATIC",
                    labels[i],
                    0, 0,
                    295, y + 4,
                    115, 25,
                    0, state->font
                );

                state->fields[i] = AddControl(
                    hwnd,
                    L"EDIT",
                    L"",
                    WS_TABSTOP |
                    ES_AUTOHSCROLL,
                    WS_EX_CLIENTEDGE,
                    410, y,
                    270, 28,
                    2100 + i,
                    state->font
                );

                SendMessageW(
                    state->fields[i],
                    EM_SETLIMITTEXT,
                    511, 0
                );
            }

            AddControl(
                hwnd, L"BUTTON",
                L"Нова фирма",
                WS_TABSTOP, 0,
                15, 380, 125, 35,
                ID_NEW_COMPANY,
                state->font
            );

            AddControl(
                hwnd, L"BUTTON",
                L"Запази",
                WS_TABSTOP, 0,
                150, 380, 125, 35,
                ID_SAVE_COMPANY,
                state->font
            );

            AddControl(
                hwnd, L"BUTTON",
                L"Избери фирма",
                WS_TABSTOP, 0,
                410, 380, 140, 35,
                ID_SELECT_COMPANY,
                state->font
            );

            AddControl(
                hwnd, L"BUTTON",
                L"Затвори",
                WS_TABSTOP, 0,
                560, 380, 120, 35,
                ID_CLOSE_COMPANY,
                state->font
            );

            RefreshList(hwnd, state);

            return 0;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case ID_COMPANY_LIST:

                    if (HIWORD(wParam) ==
                        LBN_SELCHANGE)
                    {
                        LoadSelected(state);
                    }

                    return 0;

                case ID_NEW_COMPANY:

                    ClearFields(state);
                    return 0;

                case ID_SAVE_COMPANY:

                    SaveFields(hwnd, state);
                    return 0;

                case ID_SELECT_COMPANY:
                {
                    int index = (int)SendMessageW(
                        state->list,
                        LB_GETCURSEL,
                        0, 0
                    );

                    if (index < 0 ||
                        index >=
                        (int)state->companies.size())
                    {
                        MessageBoxW(
                            hwnd,
                            L"Изберете фирма от списъка.",
                            L"AME",
                            MB_OK | MB_ICONWARNING
                        );

                        return 0;
                    }

                    state->selectedCompany = state->companies[index];

                    state->selected = true;

                    DestroyWindow(hwnd);

                    return 0;
                }

                case ID_CLOSE_COMPANY:

                    DestroyWindow(hwnd);
                    return 0;
            }

            break;
        }

        case WM_CLOSE:

            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:

            state->closed = true;
            return 0;
    }

    return DefWindowProcW(
        hwnd,
        msg,
        wParam,
        lParam
    );
}

bool ShowCompaniesWindow(
    HWND parent,
    Company& selectedCompany
)
{
    if (!InitializeCompanies())
    {
        ShowError(parent);
        return false;
    }

    const wchar_t CLASS_NAME[] =
        L"AME_CompaniesWindow";

    HINSTANCE instance =
        GetModuleHandleW(NULL);

    WNDCLASSW wc = {};

    wc.lpfnWndProc = CompaniesProc;
    wc.hInstance = instance;
    wc.lpszClassName = CLASS_NAME;

    wc.hCursor = LoadCursorW(
        NULL, IDC_ARROW
    );

    wc.hbrBackground =
        (HBRUSH)(COLOR_BTNFACE + 1);

    if (!RegisterClassW(&wc))
    {
        if (GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS)
        {
            return false;
        }
    }

    CompanyWindowState state = {};

    RECT size = {0, 0, 700, 435};

    AdjustWindowRectEx(
        &size,
        WS_OVERLAPPED |
        WS_CAPTION |
        WS_SYSMENU,
        FALSE,
        WS_EX_DLGMODALFRAME
    );

    int width = size.right - size.left;
    int height = size.bottom - size.top;

    RECT parentRect;
    GetWindowRect(parent, &parentRect);

    int x = parentRect.left +
        (parentRect.right -
         parentRect.left - width) / 2;

    int y = parentRect.top +
        (parentRect.bottom -
         parentRect.top - height) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        CLASS_NAME,
        L"AME - Управление на фирми",
        WS_OVERLAPPED |
        WS_CAPTION |
        WS_SYSMENU,
        x, y,
        width, height,
        parent,
        NULL,
        instance,
        &state
    );

    if (!hwnd)
        return false;

    EnableWindow(parent, FALSE);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg = {};

    while (!state.closed)
    {
        int result = GetMessageW(
            &msg, NULL, 0, 0
        );

        if (result <= 0)
        {
            if (result == 0)
                PostQuitMessage((int)msg.wParam);

            break;
        }

        if (!IsDialogMessageW(hwnd, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (IsWindow(hwnd))
        DestroyWindow(hwnd);

    EnableWindow(parent, TRUE);
    SetActiveWindow(parent);

    if (state.selected)
    {
        selectedCompany = state.selectedCompany;
        return true;
    }

    return false;
}