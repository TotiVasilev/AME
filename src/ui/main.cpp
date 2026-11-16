#include <windows.h>
#include "companies_window.h"
#include "accounting_window.h"

// =====================================
// AME - Accounting Made Easy
// Version 0.1.0
// =====================================

// Application colors

const COLORREF COLOR_BG = RGB(248, 250, 252);
const COLORREF COLOR_SIDEBAR = RGB(233, 238, 244);
const COLORREF COLOR_PRIMARY = RGB(36, 74, 112);
const COLORREF COLOR_TEXT = RGB(51, 65, 85);
const COLORREF COLOR_WHITE = RGB(255, 255, 255);

// Application resources

HBRUSH backgroundBrush;
HBRUSH sidebarBrush;
HBRUSH primaryBrush;
HBRUSH whiteBrush;

HFONT normalFont;
HFONT titleFont;
HFONT brandFont;

HWND statusBar;
HWND activeCompanyLabel;

std::wstring activeCompanyName;
int activeCompanyId = 0;

// Button IDs

#define ID_HOME       101
#define ID_COMPANIES  102
#define ID_OPERATIONS 103
#define ID_INVOICES   104
#define ID_REPORTS    105
#define ID_SETTINGS   106
#define ID_EXIT       107
#define ID_ABOUT      108
#define ID_ACCOUNTS   109
#define ID_OPENING 110

// Create a label

HWND CreateLabel(
    HWND parent,
    const wchar_t* text,
    int x,
    int y,
    int width,
    int height,
    HFONT font
) {
    HWND label = CreateWindowW(
        L"STATIC",
        text,
        WS_CHILD | WS_VISIBLE,
        x, y, width, height,
        parent,
        NULL,
        GetModuleHandleW(NULL),
        NULL
    );

    SendMessageW(
        label,
        WM_SETFONT,
        (WPARAM)font,
        TRUE
    );

    return label;
}

// Create a button

HWND CreateButton(
    HWND parent,
    const wchar_t* text,
    int id,
    int x,
    int y,
    int width,
    int height
) {
    HWND button = CreateWindowW(
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE |
        WS_TABSTOP | BS_OWNERDRAW,
        x, y, width, height,
        parent,
        (HMENU)(INT_PTR)id,
        GetModuleHandleW(NULL),
        NULL
    );

    SendMessageW(
        button,
        WM_SETFONT,
        (WPARAM)normalFont,
        TRUE
    );

    return button;
}

// Create the main menu

void CreateMainMenu(HWND hwnd) {

    HMENU menuBar = CreateMenu();

    HMENU fileMenu = CreatePopupMenu();

    AppendMenuW(
        fileMenu,
        MF_STRING,
        ID_EXIT,
        L"Изход"
    );

    HMENU companyMenu = CreatePopupMenu();

    AppendMenuW(
        companyMenu,
        MF_STRING,
        ID_COMPANIES,
        L"Управление на фирми"
    );

    HMENU accountingMenu = CreatePopupMenu();

    AppendMenuW(
        accountingMenu,
        MF_STRING,
        ID_OPERATIONS,
        L"Счетоводни операции"
    );

    AppendMenuW(
        accountingMenu,
        MF_STRING,
        ID_ACCOUNTS,
        L"Сметкоплан"
    );

    AppendMenuW(
        accountingMenu,
        MF_STRING,
        ID_INVOICES,
        L"Фактури"
    );

    AppendMenuW(accountingMenu, MF_STRING, ID_OPENING, L"Начални салда");
    HMENU reportsMenu = CreatePopupMenu();

    AppendMenuW(
        reportsMenu,
        MF_STRING,
        ID_REPORTS,
        L"Оборотна ведомост"
    );

    HMENU helpMenu = CreatePopupMenu();

    AppendMenuW(
        helpMenu,
        MF_STRING,
        ID_ABOUT,
        L"За AME"
    );

    AppendMenuW(
        menuBar, MF_POPUP,
        (UINT_PTR)fileMenu, L"Файл"
    );

    AppendMenuW(
        menuBar, MF_POPUP,
        (UINT_PTR)companyMenu, L"Фирми"
    );

    AppendMenuW(
        menuBar, MF_POPUP,
        (UINT_PTR)accountingMenu,
        L"Счетоводство"
    );

    AppendMenuW(
        menuBar, MF_POPUP,
        (UINT_PTR)reportsMenu, L"Справки"
    );

    AppendMenuW(
        menuBar, MF_POPUP,
        (UINT_PTR)helpMenu, L"Помощ"
    );

    SetMenu(hwnd, menuBar);
}

// Create the application interface

void CreateInterface(HWND hwnd) {

    normalFont = CreateFontW(
        -16, 0, 0, 0,
        FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH,
        L"Tahoma"
    );

    titleFont = CreateFontW(
        -28, 0, 0, 0,
        FW_BOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH,
        L"Tahoma"
    );

    brandFont = CreateFontW(
        -30, 0, 0, 0,
        FW_BOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH,
        L"Tahoma"
    );

    CreateMainMenu(hwnd);

    // Sidebar

    CreateLabel(
        hwnd, L"AME",
        20, 20, 170, 45,
        brandFont
    );

    CreateButton(
        hwnd, L"Начало",
        ID_HOME, 10, 85, 180, 40
    );

    CreateButton(
        hwnd, L"Фирми",
        ID_COMPANIES, 10, 135, 180, 40
    );

    CreateButton(
        hwnd, L"Операции",
        ID_OPERATIONS, 10, 185, 180, 40
    );

    CreateButton(
        hwnd, L"Фактури",
        ID_INVOICES, 10, 235, 180, 40
    );

    CreateButton(
        hwnd, L"Справки",
        ID_REPORTS, 10, 285, 180, 40
    );

    CreateButton(
        hwnd, L"Настройки",
        ID_SETTINGS, 10, 335, 180, 40
    );

    // Dashboard

    CreateLabel(
        hwnd, L"Работно табло",
        230, 25, 500, 45,
        titleFont
    );

    CreateLabel(
        hwnd, L"Добре дошли в AME",
        230, 75, 500, 30,
        normalFont
    );

    CreateLabel(
        hwnd, L"Активна фирма:",
        250, 140, 250, 25,
        normalFont
    );

    activeCompanyLabel = CreateLabel(
        hwnd, L"Няма избрана фирма",
        250, 175, 300, 30,
        normalFont
    );

    CreateButton(
        hwnd, L"Избери фирма",
        ID_COMPANIES, 580, 155, 170, 45
    );

    // Quick actions

    CreateButton(
        hwnd, L"Нова операция",
        ID_OPERATIONS, 230, 265, 250, 100
    );

    CreateButton(
        hwnd, L"Нова фактура",
        ID_INVOICES, 500, 265, 250, 100
    );

    CreateButton(
        hwnd, L"Сметкоплан",
        ID_ACCOUNTS, 230, 385, 250, 100
    );

    CreateButton(
        hwnd, L"Оборотна ведомост",
        ID_REPORTS, 500, 385, 250, 100
    );

    // Status bar

    statusBar = CreateLabel(
        hwnd,
        L"Версия 0.1.0\n\n",
        10, 0, 900, 25,
        normalFont
    );
}

// Placeholder for unfinished modules

void ShowModule(
    HWND hwnd,
    const wchar_t* name
) {
    MessageBoxW(
        hwnd,
        L"Този модул предстои да бъде разработен.",
        name,
        MB_OK | MB_ICONINFORMATION
    );
}

// Window procedure

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
) {
    switch (msg) {

        case WM_CREATE:

            backgroundBrush =
                CreateSolidBrush(COLOR_BG);

            sidebarBrush =
                CreateSolidBrush(COLOR_SIDEBAR);

            primaryBrush =
                CreateSolidBrush(COLOR_PRIMARY);

            whiteBrush =
                CreateSolidBrush(COLOR_WHITE);

            CreateInterface(hwnd);
            if (InitializeCompanies()) {
                Company c;
                if (RestoreActiveCompany(c)) {
                    activeCompanyId = c.id;
                    activeCompanyName = c.name;
                    SetWindowTextW(activeCompanyLabel, c.name.c_str());
                    SetWindowTextW(statusBar, (L"AME v0.1.0   |   " + c.name).c_str());
                }
            } else MessageBoxW(hwnd, GetCompanyError().c_str(), L"AME", MB_OK | MB_ICONERROR);

            return 0;

        case WM_SIZE: {

            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            MoveWindow(
                statusBar,
                10,
                height - 25,
                width - 20,
                25,
                TRUE
            );

            InvalidateRect(hwnd, NULL, TRUE);

            return 0;
        }

        case WM_GETMINMAXINFO: {

            MINMAXINFO* info =
                (MINMAXINFO*)lParam;

            info->ptMinTrackSize.x = 800;
            info->ptMinTrackSize.y = 620;

            return 0;
        }

        case WM_ERASEBKGND: {

            HDC dc = (HDC)wParam;

            RECT rect;
            GetClientRect(hwnd, &rect);

            FillRect(
                dc, &rect, backgroundBrush
            );

            RECT sidebar = {
                0, 0, 200, rect.bottom
            };

            FillRect(
                dc, &sidebar, sidebarBrush
            );

            RECT companyCard = {
                230, 120, 770, 230
            };

            FillRect(
                dc, &companyCard, whiteBrush
            );

            FrameRect(
                dc,
                &companyCard,
                (HBRUSH)GetStockObject(
                    LTGRAY_BRUSH
                )
            );

            RECT status = {
                0,
                rect.bottom - 28,
                rect.right,
                rect.bottom
            };

            FillRect(
                dc, &status, sidebarBrush
            );

            return 1;
        }

        case WM_CTLCOLORSTATIC: {

            HDC dc = (HDC)wParam;

            SetTextColor(dc, COLOR_PRIMARY);
            SetBkMode(dc, TRANSPARENT);

            return (LRESULT)GetStockObject(
                NULL_BRUSH
            );
        }

        case WM_DRAWITEM: {

            DRAWITEMSTRUCT* item =
                (DRAWITEMSTRUCT*)lParam;

            if (item->CtlType != ODT_BUTTON) {
                break;
            }

            HDC dc = item->hDC;
            RECT rect = item->rcItem;

            bool pressed =
                (item->itemState &
                 ODS_SELECTED) != 0;

            bool sidebar =
                rect.right - rect.left <= 180;

            bool primary =
                item->CtlID == ID_HOME ||
                (item->CtlID == ID_COMPANIES &&
                 rect.right - rect.left == 170);

            COLORREF bg = COLOR_WHITE;
            COLORREF fg = COLOR_PRIMARY;

            if (primary) {
                bg = COLOR_PRIMARY;
                fg = COLOR_WHITE;
            }
            else if (sidebar) {
                bg = COLOR_SIDEBAR;
            }

            if (pressed) {
                bg = RGB(190, 210, 230);
                fg = COLOR_PRIMARY;
            }

            HBRUSH brush =
                CreateSolidBrush(bg);

            FillRect(dc, &rect, brush);

            DeleteObject(brush);

            FrameRect(
                dc,
                &rect,
                (HBRUSH)GetStockObject(
                    LTGRAY_BRUSH
                )
            );

            wchar_t text[128];

            GetWindowTextW(
                item->hwndItem,
                text,
                128
            );

            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, fg);

            HFONT oldFont =
                (HFONT)SelectObject(
                    dc, normalFont
                );

            DrawTextW(
                dc,
                text,
                -1,
                &rect,
                DT_CENTER |
                DT_VCENTER |
                DT_SINGLELINE
            );

            SelectObject(dc, oldFont);

            if (item->itemState & ODS_FOCUS) {

                RECT focus = rect;

                InflateRect(&focus, -4, -4);

                DrawFocusRect(dc, &focus);
            }

            return TRUE;
        }

        case WM_COMMAND:

            switch (LOWORD(wParam)) {

                case ID_HOME:
                    return 0;

                case ID_COMPANIES:
                {
                    Company selectedCompany;

                    if (ShowCompaniesWindow(
                        hwnd,
                        selectedCompany
                    ))
                    {
                        if (!RememberActiveCompany(selectedCompany.id)) {
                            MessageBoxW(hwnd, L"Не може да се запази активната фирма.", L"AME", MB_OK | MB_ICONERROR);
                            return 0;
                        }
                        activeCompanyId = selectedCompany.id;
                        activeCompanyName = selectedCompany.name;

                        SetWindowTextW(
                            activeCompanyLabel,
                            activeCompanyName.c_str()
                        );

                        std::wstring status =
                            L"AME v0.1.0   |   " +
                            activeCompanyName;

                        SetWindowTextW(
                            statusBar,
                            status.c_str()
                        );

                        InvalidateRect(
                            hwnd,
                            NULL,
                            TRUE
                        );
                    }

                    return 0;
                }

                case ID_OPERATIONS:
                    if (!activeCompanyId) MessageBoxW(hwnd, L"Първо изберете фирма.", L"AME", MB_OK | MB_ICONWARNING);
                    else ShowJournalWindow(hwnd, activeCompanyId, activeCompanyName);
                    return 0;

                case ID_INVOICES:
                    if (!activeCompanyId) MessageBoxW(hwnd,L"Първо изберете фирма.",L"AME",MB_OK);
                    else ShowPrototypeWindow(hwnd,activeCompanyId,activeCompanyName,0);
                    return 0;

                case ID_REPORTS:
                    if (!activeCompanyId) MessageBoxW(hwnd,L"Първо изберете фирма.",L"AME",MB_OK);
                    else ShowPrototypeWindow(hwnd,activeCompanyId,activeCompanyName,1);
                    return 0;

                case ID_ACCOUNTS:
                case ID_OPENING:
                    if (!activeCompanyId) MessageBoxW(hwnd, L"Първо изберете фирма.", L"AME", MB_OK | MB_ICONWARNING);
                    else ShowAccountingWindow(hwnd, activeCompanyId, activeCompanyName, LOWORD(wParam) == ID_OPENING);
                    return 0;

                case ID_SETTINGS:
                    if (!activeCompanyId) MessageBoxW(hwnd,L"Първо изберете фирма.",L"AME",MB_OK);
                    else ShowPrototypeWindow(hwnd,activeCompanyId,activeCompanyName,2);
                    return 0;

                case ID_ABOUT:

                    MessageBoxW(
                        hwnd,
                        L"AME - Accounting Made Easy\n"
                        L"Версия 0.1.0\n\n"
                        L"Български счетоводен софтуер",
                        L"За AME",
                        MB_OK | MB_ICONINFORMATION
                    );

                    return 0;

                case ID_EXIT:
                    DestroyWindow(hwnd);
                    return 0;
            }

            break;

        case WM_DESTROY:

            DeleteObject(normalFont);
            DeleteObject(titleFont);
            DeleteObject(brandFont);

            DeleteObject(backgroundBrush);
            DeleteObject(sidebarBrush);
            DeleteObject(primaryBrush);
            DeleteObject(whiteBrush);

            PostQuitMessage(0);

            return 0;
    }

    return DefWindowProcW(
        hwnd, msg, wParam, lParam
    );
}

// Application entry point

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int show
) {
    const wchar_t CLASS_NAME[] =
        L"AME_MainWindow";

    WNDCLASSW wc = {};

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = CLASS_NAME;

    wc.hCursor = LoadCursorW(
        NULL, IDC_ARROW
    );

    wc.hIcon = LoadIconW(
        NULL, IDI_APPLICATION
    );

    if (!RegisterClassW(&wc)) {
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"AME - Accounting Made Easy",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        900,
        650,
        NULL,
        NULL,
        instance,
        NULL
    );

    if (!hwnd) {
        return 1;
    }

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg = {};

    while (
        GetMessageW(&msg, NULL, 0, 0) > 0
    ) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}