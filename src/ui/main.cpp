#include <windows.h>
#include "companies_window.h"
#include "accounting_window.h"
#include "register_window.h"
#include "documents_window.h"
#include "extra_modules.h"
#include "professional_modules.h"
#include "compliance_workbench.h"
#include "fund_calculator.h"

// =====================================
// AME - Accounting Made Easy
// Version 0.7.0
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
#define ID_REGISTER 111
#define ID_PERIOD 112
#define ID_PAYROLL 113
#define ID_DEPRECIATION 114
#define ID_EMPLOYMENT 115
#define ID_ADMIN 116
#define ID_OLD_REPORTS 117

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

// Commands for VIVA-inspired menus. The last four "Обща отчетност"
// commands and explicitly excluded modules are intentionally omitted.
#define MENU_GENERAL 2000
#define MENU_BASE 2100
#define MENU_ASSETS 2200
#define MENU_EMPLOYMENT 2300
#define MENU_WAGES 2400
#define MENU_ADMIN 2500

static const wchar_t* generalItems[]={
 L"Въвеждане на документи",L"Хронологичен опис на операциите",
 L"Оборотна ведомост месечна",L"Оборотна ведомост с натрупване",L"Справка по месеци за сметка",
 L"Журнали на сметките по дати",L"Журнали на сметките по документи",L"Извлечение от сметка обобщено месечно",
 L"Извлечение от сметка обобщено с натрупване",L"Извлечение от сметка подробно месечно",
 L"Извлечение от сметка подробно с натрупване",L"Кореспонденция по дебита и кредита",
 L"Справка по дати по сметка",L"Валутен дневник",L"Касова книга",L"Главна книга",
 L"Баланс",L"Отчет за приходите и разходите",L"Паричен поток",L"Дневник на покупките",
 L"Дневник на продажбите",L"Декларация по ДДС",L"Запис на файлове за ДДС"
};
static const wchar_t* baseItems[]={
 L"Въвеждане на сметкоплана",L"Въвеждане на сборни сметки",L"Разпечатка на сметкоплана",
 L"Номенклатура на контрагентите",L"Номенклатура на материалите",L"Номенклатура на разходите"
};
static const wchar_t* assetItems[]={
 L"Инвентарна книга",L"Начисляване на амортизациите",L"Справка за заприходените активи за месеца",
 L"Справка за отписаните активи за месеца",L"Справка за заприходените активи с натрупване",
 L"Справка за отписаните активи с натрупване",L"Справка за начислените амортизации",
 L"Инвентаризационен опис",L"Амортизационен план счетоводен",
 L"Амортизационен план данъчен",L"Сравнителна таблица"
};
static const wchar_t* employmentItems[]={
 L"Трудов договор",L"Допълнително споразумение",L"Заповед за прекратяване",
 L"Заповед за прекласиране",L"Заповед за отпуск",L"Присъствена форма",
 L"Въвеждане електронен трудов запис",L"Запис на XML файл трудов запис",
 L"Разглеждане на изпратени записи"
};
static const wchar_t* wagesItems[]={
 L"Библиотека на персонала",L"Библиотека на ведомостите",L"Библиотека на длъжностите",
 L"Библиотека на начисленията",L"Библиотека на удръжките",L"Въвеждане на начисления и удръжки",
 L"Разплащателна ведомост",L"Ведомост личен труд",L"Печат на фишове",L"Рекапитулации",
 L"Осчетоводяване на заплатите",L"Запис на Декларация 1",L"Запис на Декларация 6",
 L"Въвеждане на корекции",L"Запис на коригиращи данни",L"Запис на заличаващи данни",
 L"Печат на протокол",L"Справка за доходите",L"Удостоверение за детски",
 L"Осигурителен доход и отработени дни",L"Граждански договори",L"Таблица за ДОД и проценти",
 L"Настройка на заплатите",L"Изчисляване на дължимия данък",L"Запис на файлове чл.73 ал.6"
};
static const wchar_t* adminItems[]={
 L"Данни на фирмата ДДС",L"Данни на фирмата Заплати",L"Данни на фирмата Фактури",
 L"Корекции и изтриване на партиди",L"Настройка на баланса",L"Настройка на отчета за приходи и разходи",
 L"Настройка на паричния поток",L"Настройка на приложенията",L"Годишно приключване",
 L"Архивиране на данни",L"Възстановяване на данни",L"Архивиране на фирми",
 L"Възстановяване на фирми",L"Запис на всички фирми едновременно",
 L"Откриване на фирма",L"Дневници на Excel и справка"
};

void CreateMainMenu(HWND hwnd) {
    HMENU bar=CreateMenu();
    HMENU general=CreatePopupMenu();
    HMENU base=CreatePopupMenu();
    HMENU assets=CreatePopupMenu();
    HMENU employment=CreatePopupMenu();
    HMENU wages=CreatePopupMenu();
    HMENU admin=CreatePopupMenu();
    HMENU file=CreatePopupMenu();
    for(int i=0;i<(int)(sizeof(generalItems)/sizeof(generalItems[0]));++i)
        AppendMenuW(general,MF_STRING,MENU_GENERAL+i,generalItems[i]);
    for(int i=0;i<(int)(sizeof(baseItems)/sizeof(baseItems[0]));++i)
        AppendMenuW(base,MF_STRING,MENU_BASE+i,baseItems[i]);
    for(int i=0;i<(int)(sizeof(assetItems)/sizeof(assetItems[0]));++i)
        AppendMenuW(assets,MF_STRING,MENU_ASSETS+i,assetItems[i]);
    for(int i=0;i<(int)(sizeof(employmentItems)/sizeof(employmentItems[0]));++i)
        AppendMenuW(employment,MF_STRING,MENU_EMPLOYMENT+i,employmentItems[i]);
    for(int i=0;i<(int)(sizeof(wagesItems)/sizeof(wagesItems[0]));++i)
        AppendMenuW(wages,MF_STRING,MENU_WAGES+i,wagesItems[i]);
    for(int i=0;i<(int)(sizeof(adminItems)/sizeof(adminItems[0]));++i)
        AppendMenuW(admin,MF_STRING,MENU_ADMIN+i,adminItems[i]);
    AppendMenuW(file,MF_STRING,ID_OLD_REPORTS,L"Справки — предишен модул");
    AppendMenuW(file,MF_SEPARATOR,0,nullptr);
    AppendMenuW(file,MF_STRING,ID_EXIT,L"Изход");
    AppendMenuW(bar,MF_POPUP,(UINT_PTR)general,L"Обща отчетност");
    AppendMenuW(bar,MF_POPUP,(UINT_PTR)base,L"Счетоводна база");
    AppendMenuW(bar,MF_POPUP,(UINT_PTR)assets,L"Амортизации");
    AppendMenuW(bar,MF_POPUP,(UINT_PTR)employment,L"Назначения");
    AppendMenuW(bar,MF_POPUP,(UINT_PTR)wages,L"Заплати");
    AppendMenuW(bar,MF_POPUP,(UINT_PTR)admin,L"Администратор");
    AppendMenuW(bar,MF_POPUP,(UINT_PTR)file,L"Край");
    SetMenu(hwnd,bar);
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

    // The six daily actions requested during the VIVA demonstration.
    // Existing account and journal screens remain accessible.
    CreateLabel(hwnd, L"AME", 20, 20, 170, 45, brandFont);
    CreateButton(hwnd, L"Начало", ID_HOME, 10, 85, 180, 40);
    CreateButton(hwnd, L"Фирма", ID_COMPANIES, 10, 135, 180, 40);
    CreateButton(hwnd, L"Документи", ID_OPERATIONS, 10, 185, 180, 40);
    CreateButton(hwnd, L"Опис", ID_REGISTER, 10, 235, 180, 40);
    CreateButton(hwnd, L"Справки", ID_REPORTS, 10, 285, 180, 40);
    CreateButton(hwnd, L"Период", ID_PERIOD, 10, 335, 180, 40);
    CreateButton(hwnd, L"Заплати", ID_PAYROLL, 10, 385, 180, 40);

    CreateLabel(hwnd, L"Счетоводно работно място", 230, 22, 780, 46, titleFont);
    CreateLabel(hwnd, L"AME — основните действия за избраната фирма", 230, 74, 760, 28, normalFont);
    CreateLabel(hwnd, L"Активна фирма:", 250, 139, 250, 25, normalFont);
    activeCompanyLabel = CreateLabel(hwnd, L"Няма избрана фирма", 250, 173, 550, 30, normalFont);
    CreateButton(hwnd, L"Избери фирма", ID_COMPANIES, 880, 155, 175, 45);

    CreateButton(hwnd, L"Фирма", ID_COMPANIES, 230, 265, 260, 90);
    CreateButton(hwnd, L"Документи", ID_OPERATIONS, 515, 265, 260, 90);
    CreateButton(hwnd, L"Опис", ID_REGISTER, 800, 265, 260, 90);
    CreateButton(hwnd, L"Справки", ID_REPORTS, 230, 375, 260, 90);
    CreateButton(hwnd, L"Период", ID_PERIOD, 515, 375, 260, 90);
    CreateButton(hwnd, L"Заплати / персонал", ID_PAYROLL, 800, 375, 260, 90);

    CreateLabel(hwnd, L"Счетоводни инструменти", 230, 491, 620, 29, normalFont);
    CreateButton(hwnd, L"Сметкоплан", ID_ACCOUNTS, 230, 530, 260, 62);
    CreateButton(hwnd, L"Начални салда", ID_OPENING, 515, 530, 260, 62);
    CreateButton(hwnd, L"Ръчен журнал", ID_INVOICES, 800, 530, 260, 62);

    // Status bar

    statusBar = CreateLabel(
        hwnd,
        L"AME v0.7.0   |   Работна версия",
        10, 0, 1100, 25,
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
                    SetWindowTextW(statusBar, (L"AME v0.7.0   |   " + c.name).c_str());
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

            info->ptMinTrackSize.x = 1100;
            info->ptMinTrackSize.y = 700;

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
                230, 120, 1075, 230
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

            {
                int selected=(int)LOWORD(wParam);
                if (selected>=MENU_GENERAL && selected<MENU_GENERAL+23) {
                    if (!activeCompanyId) MessageBoxW(hwnd,L"Първо изберете фирма.",L"AME",MB_OK);
                    else if(selected==MENU_GENERAL) ShowDocumentsWindow(hwnd,activeCompanyId,activeCompanyName);
                    else if(selected==MENU_GENERAL+1) ShowRegisterWindow(hwnd,activeCompanyId,activeCompanyName);
                    else if(selected>=MENU_GENERAL+19) {
                        if(selected>=MENU_GENERAL+21) ShowAMECompliance(hwnd,activeCompanyId,activeCompanyName,0);
                        else ShowAMEProfessional(hwnd,activeCompanyId,activeCompanyName,0,selected-MENU_GENERAL-19);
                    }
                    else ShowAMEReport(hwnd,activeCompanyId,activeCompanyName,selected-MENU_GENERAL-1);
                    return 0;
                }
                if (selected>=MENU_BASE && selected<MENU_BASE+6) {
                    if (!activeCompanyId) MessageBoxW(hwnd,L"Първо изберете фирма.",L"AME",MB_OK);
                    else if(selected==MENU_BASE) ShowAccountingWindow(hwnd,activeCompanyId,activeCompanyName,false);
                    else if(selected==MENU_BASE+3) ShowAMEDirectory(hwnd,activeCompanyId,activeCompanyName,0);
                    else if(selected==MENU_BASE+4) ShowAMEDirectory(hwnd,activeCompanyId,activeCompanyName,4);
                    else if(selected==MENU_BASE+5) ShowAMEDirectory(hwnd,activeCompanyId,activeCompanyName,1);
                    else MessageBoxW(hwnd,L"Сборни сметки и печат на сметкоплана изискват отделна реализация.",L"AME — непълна функция",MB_OK|MB_ICONWARNING);
                    return 0;
                }
                if (selected>=MENU_ASSETS && selected<MENU_ASSETS+11) {
                    if (!activeCompanyId) MessageBoxW(hwnd,L"Първо изберете фирма.",L"AME",MB_OK);
                    else if(selected==MENU_ASSETS) ShowAMEDirectory(hwnd,activeCompanyId,activeCompanyName,3);
                    else if(selected==MENU_ASSETS+1 || selected==MENU_ASSETS+9) ShowAMECompliance(hwnd,activeCompanyId,activeCompanyName,4);
                    else if(selected==MENU_ASSETS+6 || selected==MENU_ASSETS+8 || selected==MENU_ASSETS+10) ShowAMEProfessional(hwnd,activeCompanyId,activeCompanyName,3);
                    else if(selected==MENU_ASSETS+7) ShowAMEReport(hwnd,activeCompanyId,activeCompanyName,22);
                    else if(selected==MENU_ASSETS+2) ShowAMEReport(hwnd,activeCompanyId,activeCompanyName,24);
                    else if(selected==MENU_ASSETS+4) ShowAMEReport(hwnd,activeCompanyId,activeCompanyName,25);
                    else MessageBoxW(hwnd,L"Този вид амортизационна справка още не е изчислително реализиран.",L"AME — непълна функция",MB_OK|MB_ICONWARNING);
                    return 0;
                }
                if (selected>=MENU_EMPLOYMENT && selected<MENU_EMPLOYMENT+9) {
                    if (!activeCompanyId) MessageBoxW(hwnd,L"Първо изберете фирма.",L"AME",MB_OK);
                    else {
                        if(selected==MENU_EMPLOYMENT+7) ShowAMECompliance(hwnd,activeCompanyId,activeCompanyName,3);
                        else ShowAMEProfessional(hwnd,activeCompanyId,activeCompanyName,2,selected-MENU_EMPLOYMENT);
                    }
                    return 0;
                }
                if (selected>=MENU_WAGES && selected<MENU_WAGES+25) {
                    if (!activeCompanyId) MessageBoxW(hwnd,L"Първо изберете фирма.",L"AME",MB_OK);
                    else if(selected==MENU_WAGES) ShowAMEDirectory(hwnd,activeCompanyId,activeCompanyName,2);
                    else if(selected==MENU_WAGES+2) ShowAMEDirectory(hwnd,activeCompanyId,activeCompanyName,6);
                    else if(selected==MENU_WAGES+3) ShowAMEDirectory(hwnd,activeCompanyId,activeCompanyName,7);
                    else if(selected==MENU_WAGES+4) ShowAMEDirectory(hwnd,activeCompanyId,activeCompanyName,8);
                    else if(selected==MENU_WAGES+22) ShowAMEFundCalculator(hwnd,activeCompanyId,activeCompanyName);
                    else if(selected==MENU_WAGES+5) ShowAMECompliance(hwnd,activeCompanyId,activeCompanyName,1);
                    else if(selected==MENU_WAGES+11 || selected==MENU_WAGES+12 || selected==MENU_WAGES+14 || selected==MENU_WAGES+15) ShowAMECompliance(hwnd,activeCompanyId,activeCompanyName,2);
                    else {
                        if(selected==MENU_WAGES+11||selected==MENU_WAGES+12||selected==MENU_WAGES+14||selected==MENU_WAGES+15||selected==MENU_WAGES+24)
                            MessageBoxW(hwnd,L"Декларации Д1/Д6 и файлове по чл. 73 не се генерират в тази версия. Отваря се вътрешната ведомост за работа и проверка.",L"AME — няма официален НАП експорт",MB_OK|MB_ICONWARNING);
                        ShowAMEProfessional(hwnd,activeCompanyId,activeCompanyName,1,selected-MENU_WAGES);
                    }
                    return 0;
                }
                if (selected>=MENU_ADMIN && selected<MENU_ADMIN+16) {
                    if (!activeCompanyId) MessageBoxW(hwnd,L"Първо изберете фирма.",L"AME",MB_OK);
                    else if(selected==MENU_ADMIN+8) ShowAMECompliance(hwnd,activeCompanyId,activeCompanyName,5);
                    else if(selected==MENU_ADMIN+9 || selected==MENU_ADMIN+10 || selected==MENU_ADMIN+11 || selected==MENU_ADMIN+12) ShowPrototypeWindow(hwnd,activeCompanyId,activeCompanyName,2);
                    else if(selected==MENU_ADMIN+14) SendMessageW(hwnd,WM_COMMAND,ID_COMPANIES,0);
                    else if(selected==MENU_ADMIN+15) ShowAMEReport(hwnd,activeCompanyId,activeCompanyName,18);
                    else MessageBoxW(hwnd,L"Избраната административна функция все още не е реализирана.",L"AME — непълна функция",MB_OK|MB_ICONWARNING);
                    return 0;
                }
            }
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
                            L"AME v0.7.0   |   " +
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
                    else ShowDocumentsWindow(hwnd, activeCompanyId, activeCompanyName);
                    return 0;

                case ID_INVOICES:
                    if (!activeCompanyId) MessageBoxW(hwnd,L"Първо изберете фирма.",L"AME",MB_OK);
                    else ShowJournalWindow(hwnd,activeCompanyId,activeCompanyName);
                    return 0;

                case ID_REGISTER:
                    if (!activeCompanyId) MessageBoxW(hwnd,L"Първо изберете фирма.",L"AME",MB_OK | MB_ICONWARNING);
                    else ShowRegisterWindow(hwnd,activeCompanyId,activeCompanyName);
                    return 0;

                case ID_REPORTS:
                    if (!activeCompanyId) MessageBoxW(hwnd,L"Първо изберете фирма.",L"AME",MB_OK);
                    else ShowAMEReport(hwnd,activeCompanyId,activeCompanyName,1);
                    return 0;

                case ID_OLD_REPORTS:
                    if (!activeCompanyId) MessageBoxW(hwnd,L"Първо изберете фирма.",L"AME",MB_OK);
                    else ShowPrototypeWindow(hwnd,activeCompanyId,activeCompanyName,1);
                    return 0;

                case ID_ACCOUNTS:
                case ID_OPENING:
                    if (!activeCompanyId) MessageBoxW(hwnd, L"Първо изберете фирма.", L"AME", MB_OK | MB_ICONWARNING);
                    else ShowAccountingWindow(hwnd, activeCompanyId, activeCompanyName, LOWORD(wParam) == ID_OPENING);
                    return 0;

                case ID_PERIOD:
                    if (!activeCompanyId) MessageBoxW(hwnd,L"Първо изберете фирма.",L"AME",MB_OK);
                    else ShowAMEPeriod(hwnd,activeCompanyId,activeCompanyName);
                    return 0;

                case ID_PAYROLL:
                    if (!activeCompanyId) MessageBoxW(hwnd,L"Първо изберете фирма.",L"AME",MB_OK);
                    else ShowAMEProfessional(hwnd,activeCompanyId,activeCompanyName,1);
                    return 0;

                case ID_SETTINGS:
                    if (!activeCompanyId) MessageBoxW(hwnd,L"Първо изберете фирма.",L"AME",MB_OK);
                    else ShowPrototypeWindow(hwnd,activeCompanyId,activeCompanyName,2);
                    return 0;

                case ID_ABOUT:

                    MessageBoxW(
                        hwnd,
                        L"AME - Accounting Made Easy\n"
                        L"Версия 0.7.0\n\n"
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
        1130,
        740,
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