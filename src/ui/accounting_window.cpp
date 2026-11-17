#include "accounting_window.h"
#include "../data/ame_calculations.h"
#include "../data/chart_template.h"
#include <sqlite3.h>
#include <shlobj.h>
#include <vector>
#include <cstdlib>
#include <cwchar>
#include <limits>

namespace {
struct Db { sqlite3* p = nullptr; ~Db(){ if(p) sqlite3_close(p); } };
struct Stmt { sqlite3_stmt* p = nullptr; ~Stmt(){ if(p) sqlite3_finalize(p); } };
std::wstring root(){ wchar_t b[MAX_PATH]; if(FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE,NULL,SHGFP_TYPE_CURRENT,b))) return L""; std::wstring s=b; s+=L"\\AME"; if(!CreateDirectoryW(s.c_str(),NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS) return L""; return s; }
bool open(Db& d,const std::wstring& path){ if(path.empty()||sqlite3_open16(path.c_str(),&d.p)!=SQLITE_OK) return false; sqlite3_busy_timeout(d.p,3000); return true; }
bool exec(Db& d,const char* sql){return sqlite3_exec(d.p,sql,NULL,NULL,NULL)==SQLITE_OK;}
bool prep(Db& d,Stmt& s,const char* sql){return sqlite3_prepare_v2(d.p,sql,-1,&s.p,NULL)==SQLITE_OK;}
std::wstring txt(sqlite3_stmt* s,int i){auto p=sqlite3_column_text16(s,i); return p?static_cast<const wchar_t*>(p):L"";}
bool bind(Stmt& s,int i,const std::wstring& v){return sqlite3_bind_text16(s.p,i,v.c_str(),-1,SQLITE_TRANSIENT)==SQLITE_OK;}
bool settings(Db& d){auto r=root(); return !r.empty()&&open(d,r+L"\\settings.db")&&exec(d,"CREATE TABLE IF NOT EXISTS settings(id INTEGER PRIMARY KEY CHECK(id=1), company_id INTEGER NOT NULL)");}
bool ledger(Db& d,int id){auto r=root(); if(r.empty()||id<=0)return false; r+=L"\\companies"; if(!CreateDirectoryW(r.c_str(),NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return false; return open(d,r+L"\\company_"+std::to_wstring(id)+L".db") && EnsureChartTemplate(d.p);}
const wchar_t* kinds[]={L"Актив",L"Пасив",L"Капитал",L"Приход",L"Разход"};
struct Account{int id; std::wstring code,name; int kind,active;};
struct State{Db db; bool opening=false; HWND list=nullptr,code=nullptr,name=nullptr,kind=nullptr,active=nullptr,date=nullptr,debit=nullptr,credit=nullptr,total=nullptr,search=nullptr,division=nullptr,summary=nullptr; HFONT font=nullptr; std::vector<Account> rows; std::vector<int> visible; int selected=0;};
void error(HWND h,const wchar_t* s){MessageBoxW(h,s,L"AME",MB_OK|MB_ICONERROR);}
std::wstring get(HWND h){int n=GetWindowTextLengthW(h); std::vector<wchar_t> b(n+1); GetWindowTextW(h,b.data(),n+1); return b.data();}
HWND ctl(HWND h,State* s,const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int ht,int id=0){HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,ht,h,(HMENU)(INT_PTR)id,GetModuleHandleW(NULL),NULL); SendMessageW(c,WM_SETFONT,(WPARAM)s->font,TRUE); return c;}
void label(HWND h,State* s,const wchar_t* t,int x,int y,int w=180){ctl(h,s,L"STATIC",t,0,x,y,w,23);}
HWND field(HWND h,State* s,int x,int y,int w,int id=0){return ctl(h,s,L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,x,y,w,25,id);}
void button(HWND h,State* s,const wchar_t* t,int x,int y,int w,int id){ctl(h,s,L"BUTTON",t,WS_TABSTOP|BS_PUSHBUTTON,x,y,w,32,id);}
std::wstring money(sqlite3_int64 v){return std::to_wstring(v/100)+L"."+(v%100<10?L"0":L"")+std::to_wstring(v%100);}
bool amount(const std::wstring& t,sqlite3_int64& v){v=0; if(t.empty())return true; int decimals=-1; bool digit=false; for(wchar_t c:t){if(c==L'.'||c==L','){if(decimals>=0)return false; decimals=0;continue;} if(c<L'0'||c>L'9'||decimals==2)return false; digit=true; if(v> (900000000000000LL-(c-L'0'))/10)return false; v=v*10+c-L'0'; if(decimals>=0)++decimals;} if(!digit)return false; if(decimals<=0){if(v>9000000000000LL)return false;v*=100;} else if(decimals==1){if(v>90000000000000LL)return false;v*=10;} return v<=900000000000000LL;}
bool dateok(const std::wstring& t){if(t.size()!=10||t[4]!=L'-'||t[7]!=L'-')return false; for(int i=0;i<10;++i)if(i!=4&&i!=7&&(t[i]<L'0'||t[i]>L'9'))return false; int y=_wtoi(t.substr(0,4).c_str()),m=_wtoi(t.substr(5,2).c_str()),d=_wtoi(t.substr(8,2).c_str()); int days[]={31,28,31,30,31,30,31,31,30,31,30,31}; return y>=1900&&m>=1&&m<=12&&d>=1&&d<=days[m-1]+(m==2&&y%4==0&&(y%100!=0||y%400==0));}
void totals(HWND h,State* s){if(!s->opening)return; auto date=get(s->date); if(!dateok(date)){SetWindowTextW(s->total,L"Въведете валидна дата (ГГГГ-ММ-ДД).");return;} Stmt q; if(!prep(s->db,q,"SELECT COALESCE(SUM(debit),0),COALESCE(SUM(credit),0) FROM opening WHERE date=?")||!bind(q,1,date)||sqlite3_step(q.p)!=SQLITE_ROW){error(h,L"Грешка при изчисляване на салдата.");return;} auto d=sqlite3_column_int64(q.p,0),c=sqlite3_column_int64(q.p,1); auto t=L"Дебит: "+money(d)+L"   Кредит: "+money(c)+(d==c?L"   | Равни":L"   | НЕРАВНИ"); SetWindowTextW(s->total,t.c_str());}
void clear(State* s){s->selected=0; SendMessageW(s->list,LB_SETCURSEL,(WPARAM)-1,0); if(!s->opening){SetWindowTextW(s->code,L"");SetWindowTextW(s->name,L"");SendMessageW(s->kind,CB_SETCURSEL,0,0);SendMessageW(s->active,BM_SETCHECK,BST_CHECKED,0);}else{SetWindowTextW(s->debit,L"0.00");SetWindowTextW(s->credit,L"0.00");}}
std::wstring lowercase(std::wstring value) {
    if (!value.empty()) CharLowerBuffW(&value[0],static_cast<DWORD>(value.size()));
    return value;
}
void drawAccounts(State* s) {
    if (!s->list) return;
    std::wstring query=s->search?lowercase(get(s->search)):L"";
    int group=s->division?static_cast<int>(SendMessageW(s->division,CB_GETCURSEL,0,0)):0;
    s->visible.clear();
    SendMessageW(s->list,WM_SETREDRAW,FALSE,0);
    SendMessageW(s->list,LB_RESETCONTENT,0,0);
    for(size_t i=0;i<s->rows.size();++i) {
        const Account& a=s->rows[i];
        if(group>0 && (a.code.empty() || a.code[0]!=static_cast<wchar_t>(L'0'+group))) continue;
        if(!query.empty() && lowercase(a.code).find(query)==std::wstring::npos &&
           lowercase(a.name).find(query)==std::wstring::npos) continue;
        std::wstring label=a.code+L"   "+a.name+(a.active?L"":L" (неактивна)");
        SendMessageW(s->list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
        s->visible.push_back(static_cast<int>(i));
    }
    SendMessageW(s->list,WM_SETREDRAW,TRUE,0);
    InvalidateRect(s->list,nullptr,TRUE);
    if(s->summary) {
        std::wstring status=L"Общо: "+std::to_wstring(s->rows.size())+
                            L"   Показани: "+std::to_wstring(s->visible.size())+
                            L"   |   Индивидуален сметкоплан на фирмата";
        SetWindowTextW(s->summary,status.c_str());
    }
}
void reload(HWND h,State* s){
    Stmt q;
    if(!prep(s->db,q,"SELECT id,code,name,kind,active FROM accounts ORDER BY code")) {
        error(h,L"Грешка при зареждане."); return;
    }
    s->rows.clear();
    int rc;
    while((rc=sqlite3_step(q.p))==SQLITE_ROW) {
        Account a={sqlite3_column_int(q.p,0),txt(q.p,1),txt(q.p,2),
                   sqlite3_column_int(q.p,3),sqlite3_column_int(q.p,4)};
        s->rows.push_back(a);
    }
    if(rc!=SQLITE_DONE)error(h,L"Грешка при зареждане.");
    drawAccounts(s);
    clear(s);
    totals(h,s);
}
void select(HWND h,State* s){int i=(int)SendMessageW(s->list,LB_GETCURSEL,0,0);if(i<0||i>=(int)s->visible.size())return;auto& a=s->rows[s->visible[i]];s->selected=a.id;if(!s->opening){SetWindowTextW(s->code,a.code.c_str());SetWindowTextW(s->name,a.name.c_str());SendMessageW(s->kind,CB_SETCURSEL,a.kind,0);SendMessageW(s->active,BM_SETCHECK,a.active?BST_CHECKED:BST_UNCHECKED,0);}else{SetWindowTextW(s->debit,L"0.00");SetWindowTextW(s->credit,L"0.00");Stmt q;if(!prep(s->db,q,"SELECT debit,credit FROM opening WHERE account_id=? AND date=?")||sqlite3_bind_int(q.p,1,a.id)!=SQLITE_OK||!bind(q,2,get(s->date))){error(h,L"Грешка при зареждане на салдо.");return;}int rc=sqlite3_step(q.p);if(rc==SQLITE_ROW){SetWindowTextW(s->debit,money(sqlite3_column_int64(q.p,0)).c_str());SetWindowTextW(s->credit,money(sqlite3_column_int64(q.p,1)).c_str());}else if(rc!=SQLITE_DONE)error(h,L"Грешка при зареждане на салдо.");}}
void save(HWND h,State* s){Stmt q;if(!s->opening){auto code=get(s->code),name=get(s->name);int k=(int)SendMessageW(s->kind,CB_GETCURSEL,0,0);if(code.empty()||name.find_first_not_of(L" \t")==std::wstring::npos||k<0||k>4||code.find_first_not_of(L"0123456789.")!=std::wstring::npos||code.front()==L'.'||code.back()==L'.'||code.find(L"..")!=std::wstring::npos){error(h,L"Въведете номер, име и вид на сметката.");return;}if(!prep(s->db,q,s->selected?"UPDATE accounts SET code=?,name=?,kind=?,active=? WHERE id=?":"INSERT INTO accounts(code,name,kind,active) VALUES(?,?,?,?)")||!bind(q,1,code)||!bind(q,2,name)||sqlite3_bind_int(q.p,3,k)!=SQLITE_OK||sqlite3_bind_int(q.p,4,SendMessageW(s->active,BM_GETCHECK,0,0)==BST_CHECKED)!=SQLITE_OK||(s->selected&&sqlite3_bind_int(q.p,5,s->selected)!=SQLITE_OK)){error(h,L"Грешка при запис.");return;}}else{sqlite3_int64 d,c;auto date=get(s->date);if(!s->selected||!dateok(date)||!amount(get(s->debit),d)||!amount(get(s->credit),c)||(d&&c)){error(h,L"Изберете сметка, валидна дата и неотрицателно салдо само по дебит или кредит.");return;}if(!prep(s->db,q,"INSERT OR REPLACE INTO opening(account_id,date,debit,credit) VALUES(?,?,?,?)")||sqlite3_bind_int(q.p,1,s->selected)!=SQLITE_OK||!bind(q,2,date)||sqlite3_bind_int64(q.p,3,d)!=SQLITE_OK||sqlite3_bind_int64(q.p,4,c)!=SQLITE_OK){error(h,L"Грешка при запис.");return;}}if(sqlite3_step(q.p)!=SQLITE_DONE){error(h,L"Записът е неуспешен. Проверете за дублиран номер или грешка в базата.");return;}reload(h,s);}
void seed(HWND h,State* s) {
    if(MessageBoxW(h,
        L"Да се добавят липсващите сметки от вградения български сметкоплан?\n"
        L"Вашите сметки, имена, салда и операции няма да бъдат променени.",
        L"AME — Сметкоплан",MB_YESNO|MB_ICONQUESTION)!=IDYES) return;
    int count=0;
    std::wstring problem;
    if(!AddMissingTemplateAccounts(s->db.p,&count,&problem)) {
        error(h,(L"Неуспешно добавяне на сметки. "+problem).c_str()); return;
    }
    reload(h,s);
    std::wstring result=L"Добавени сметки: "+std::to_wstring(count)+
                        L"\nНаличните сметки не са променени.";
    MessageBoxW(h,result.c_str(),L"AME — Сметкоплан",MB_OK|MB_ICONINFORMATION);
}
LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
    State* s=(State*)GetWindowLongPtrW(h,GWLP_USERDATA);
    if(m==WM_NCCREATE){s=(State*)((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)s);}
    if(!s)return DefWindowProcW(h,m,w,l);
    switch(m){
    case WM_CREATE:{
        s->font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,L"Tahoma");
        if(s->opening){
            label(h,s,L"Изберете сметка и дата",15,15,350);
            s->list=ctl(h,s,L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|LBS_NOTIFY|WS_TABSTOP,15,45,345,310,1);
            label(h,s,L"Дата (ГГГГ-ММ-ДД)",380,45,280);
            s->date=field(h,s,380,70,270,2);
            SYSTEMTIME t;GetLocalTime(&t);wchar_t b[32];
            wsprintfW(b,L"%04d-01-01",t.wYear);SetWindowTextW(s->date,b);
            label(h,s,L"Дебит",380,110);s->debit=field(h,s,380,135,270);
            label(h,s,L"Кредит",380,175);s->credit=field(h,s,380,200,270);
            s->total=ctl(h,s,L"STATIC",L"",0,15,370,650,45);
            button(h,s,L"Зареди дата",380,245,130,6);
            button(h,s,L"Запази",380,310,130,3);
            button(h,s,L"Затвори",520,310,130,7);
        }else{
            label(h,s,L"Сметкоплан — готов за използване и редактиране",15,15,660);
            label(h,s,L"Търси номер или име",15,51,320);
            s->search=field(h,s,15,77,305,8);
            label(h,s,L"Раздел",340,51,160);
            s->division=ctl(h,s,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,340,77,170,220,9);
            const wchar_t* groups[]={L"Всички",L"1 — Капитал",L"2 — Активи",L"3 — Запаси",
                L"4 — Разчети",L"5 — Парични средства",L"6 — Разходи",L"7 — Приходи",L"8 — Други",L"9 — Задбалансови"};
            for(auto name:groups)SendMessageW(s->division,CB_ADDSTRING,0,(LPARAM)name);
            SendMessageW(s->division,CB_SETCURSEL,0,0);
            s->list=ctl(h,s,L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|LBS_NOTIFY|WS_TABSTOP,15,112,495,375,1);
            s->summary=ctl(h,s,L"STATIC",L"",0,15,496,690,24);
            label(h,s,L"Номер на сметка",535,105,250);s->code=field(h,s,535,130,290);
            label(h,s,L"Наименование",535,171,250);s->name=field(h,s,535,196,290);
            label(h,s,L"Вид",535,236,250);
            s->kind=ctl(h,s,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,535,261,290,180);
            for(auto kind:kinds)SendMessageW(s->kind,CB_ADDSTRING,0,(LPARAM)kind);
            s->active=ctl(h,s,L"BUTTON",L"Активна сметка",BS_AUTOCHECKBOX|WS_TABSTOP,535,304,270,25);
            button(h,s,L"Нова сметка",535,352,140,4);
            button(h,s,L"Запази",685,352,140,3);
            button(h,s,L"Добави липсващите стандартни сметки",15,520,340,5);
            button(h,s,L"Затвори",685,520,140,7);
        }
        reload(h,s);return 0;
    }
    case WM_COMMAND:switch(LOWORD(w)){
        case 1:if(HIWORD(w)==LBN_SELCHANGE)select(h,s);return 0;
        case 2:if(HIWORD(w)==EN_CHANGE&&s->total){clear(s);totals(h,s);}return 0;
        case 3:save(h,s);return 0;
        case 4:clear(s);return 0;
        case 5:seed(h,s);return 0;
        case 6:select(h,s);totals(h,s);return 0;
        case 7:DestroyWindow(h);return 0;
        case 8:if(HIWORD(w)==EN_CHANGE){drawAccounts(s);clear(s);}return 0;
        case 9:if(HIWORD(w)==CBN_SELCHANGE){drawAccounts(s);clear(s);}return 0;
    }break;
    case WM_DESTROY:if(s->font)DeleteObject(s->font);return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
}
bool RememberActiveCompany(int id){Db d,company;if(!ledger(company,id)||!settings(d))return false;Stmt q;return prep(d,q,"INSERT OR REPLACE INTO settings(id,company_id) VALUES(1,?)")&&sqlite3_bind_int(q.p,1,id)==SQLITE_OK&&sqlite3_step(q.p)==SQLITE_DONE;}
bool RestoreActiveCompany(Company& c){Db d;if(!settings(d))return false;Stmt q;if(!prep(d,q,"SELECT company_id FROM settings WHERE id=1")||sqlite3_step(q.p)!=SQLITE_ROW)return false;int id=sqlite3_column_int(q.p,0);std::vector<Company> rows;if(!LoadCompanies(rows))return false;for(auto& a:rows)if(a.id==id){Db company;if(!ledger(company,id))return false;c=a;return true;}return false;}
void ShowAccountingWindow(HWND parent,int id,const std::wstring& name,bool opening){State s;s.opening=opening;if(!ledger(s.db,id)){error(parent,L"Не може да се отвори счетоводната база.");return;}WNDCLASSW wc={};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(NULL);wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.lpszClassName=L"AMEAccounting004";RegisterClassW(&wc);auto title=std::wstring(opening?L"Начални салда — ":L"Сметкоплан — ")+name;HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,title.c_str(),WS_CAPTION|WS_SYSMENU|WS_POPUP, CW_USEDEFAULT,CW_USEDEFAULT,opening?690:855,opening?460:600,parent,NULL,wc.hInstance,&s);if(!h){error(parent,L"Не може да се отвори прозорецът.");return;}EnableWindow(parent,FALSE);ShowWindow(h,SW_SHOW);MSG msg;while(IsWindow(h)){int r=GetMessageW(&msg,NULL,0,0);if(r<=0){if(r==0)PostQuitMessage((int)msg.wParam);break;}if(!IsDialogMessageW(h,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}if(IsWindow(h))DestroyWindow(h);EnableWindow(parent,TRUE);SetActiveWindow(parent);}

namespace {
struct JournalLine { int account; sqlite3_int64 debit,credit; };
struct Journal { Db db; std::int64_t initialEntryId=0; HFONT font=nullptr; HWND list=nullptr,lines=nullptr,account=nullptr,date=nullptr,doc=nullptr,desc=nullptr,debit=nullptr,credit=nullptr,total=nullptr,filter=nullptr; std::vector<Account> accounts; std::vector<JournalLine> rows; std::vector<sqlite3_int64> ids; sqlite3_int64 id=0; bool dirty=false,loading=false; };
HWND jc(HWND h,Journal* s,const wchar_t* cls,const wchar_t* t,DWORD style,int x,int y,int w,int ht,int id=0){ HWND c=CreateWindowExW(0,cls,t,WS_CHILD|WS_VISIBLE|style,x,y,w,ht,h,(HMENU)(INT_PTR)id,GetModuleHandleW(NULL),NULL);SendMessageW(c,WM_SETFONT,(WPARAM)s->font,TRUE);return c; }
void jl(HWND h,Journal* s,const wchar_t* t,int x,int y,int w){jc(h,s,L"STATIC",t,0,x,y,w,22);}
HWND je(HWND h,Journal* s,int x,int y,int w,int id){return jc(h,s,L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,x,y,w,25,id);}
void jb(HWND h,Journal* s,const wchar_t* t,int x,int y,int w,int id){jc(h,s,L"BUTTON",t,WS_TABSTOP|BS_PUSHBUTTON,x,y,w,30,id);}
bool schema(Db& d){return exec(d,"CREATE TABLE IF NOT EXISTS journal_entries(id INTEGER PRIMARY KEY,date TEXT NOT NULL,document TEXT NOT NULL,description TEXT NOT NULL); CREATE TABLE IF NOT EXISTS journal_lines(entry_id INTEGER NOT NULL REFERENCES journal_entries(id),line_no INTEGER NOT NULL,account_id INTEGER NOT NULL REFERENCES accounts(id),debit INTEGER NOT NULL CHECK(debit>=0),credit INTEGER NOT NULL CHECK(credit>=0),CHECK((debit>0 AND credit=0) OR (credit>0 AND debit=0)),PRIMARY KEY(entry_id,line_no)); CREATE INDEX IF NOT EXISTS journal_date ON journal_entries(date); CREATE INDEX IF NOT EXISTS journal_account ON journal_lines(account_id);");}
bool jaccounts(Journal* s){Stmt q;if(!prep(s->db,q,"SELECT id,code,name,kind,active FROM accounts ORDER BY code"))return false;int rc;while((rc=sqlite3_step(q.p))==SQLITE_ROW){Account a={sqlite3_column_int(q.p,0),txt(q.p,1),txt(q.p,2),sqlite3_column_int(q.p,3),sqlite3_column_int(q.p,4)};s->accounts.push_back(a);auto t=a.code+L" "+a.name+(a.active?L"":L" (неактивна)");SendMessageW(s->account,CB_ADDSTRING,0,(LPARAM)t.c_str());}return rc==SQLITE_DONE;}
bool jtot(Journal* s,sqlite3_int64& d,sqlite3_int64& c){d=c=0;for(auto& r:s->rows){if(r.debit>900000000000000LL-d||r.credit>900000000000000LL-c)return false;d+=r.debit;c+=r.credit;}return true;}
void jdraw(Journal* s){SendMessageW(s->lines,LB_RESETCONTENT,0,0);for(auto& r:s->rows){std::wstring t;for(auto& a:s->accounts)if(a.id==r.account)t=a.code+L" "+a.name; t+=L" | Д: "+money(r.debit)+L" | К: "+money(r.credit);SendMessageW(s->lines,LB_ADDSTRING,0,(LPARAM)t.c_str());}sqlite3_int64 d,c;bool ok=jtot(s,d,c);auto t=ok?L"Дебит: "+money(d)+L"   Кредит: "+money(c)+(d==c&&d>0?L"  | Равни":L"  | Небалансирани"):L"Превишена максимална сума";SetWindowTextW(s->total,t.c_str());}
bool jlist(Journal* s){Stmt q;if(!prep(s->db,q,"SELECT e.id,e.date,e.document,e.description,COALESCE(SUM(l.debit),0) FROM journal_entries e LEFT JOIN journal_lines l ON l.entry_id=e.id WHERE (?='' OR instr(e.date,?)>0 OR instr(e.document,?)>0 OR instr(e.description,?)>0 OR EXISTS(SELECT 1 FROM journal_lines x JOIN accounts a ON a.id=x.account_id WHERE x.entry_id=e.id AND instr(a.code,?)>0)) GROUP BY e.id ORDER BY e.date DESC,e.id DESC"))return false;auto f=get(s->filter);for(int i=1;i<=5;++i)if(!bind(q,i,f))return false;s->ids.clear();SendMessageW(s->list,LB_RESETCONTENT,0,0);int rc;while((rc=sqlite3_step(q.p))==SQLITE_ROW){s->ids.push_back(sqlite3_column_int64(q.p,0));auto t=txt(q.p,1)+L" | "+txt(q.p,2)+L" | "+txt(q.p,3)+L" | "+money(sqlite3_column_int64(q.p,4));SendMessageW(s->list,LB_ADDSTRING,0,(LPARAM)t.c_str());}return rc==SQLITE_DONE;}
bool discard(HWND h,Journal* s){return !s->dirty||MessageBoxW(h,L"Има незаписани промени. Да бъдат ли отхвърлени?",L"AME",MB_YESNO|MB_ICONQUESTION)==IDYES;}
void jnew(Journal* s){s->loading=true;s->id=0;s->rows.clear();SYSTEMTIME t;GetLocalTime(&t);wchar_t b[32];wsprintfW(b,L"%04d-%02d-%02d",t.wYear,t.wMonth,t.wDay);SetWindowTextW(s->date,b);SetWindowTextW(s->doc,L"");SetWindowTextW(s->desc,L"");SetWindowTextW(s->debit,L"");SetWindowTextW(s->credit,L"");SendMessageW(s->account,CB_SETCURSEL,(WPARAM)-1,0);jdraw(s);s->loading=false;s->dirty=false;}
bool jload(Journal* s,sqlite3_int64 id){Stmt q;if(!prep(s->db,q,"SELECT date,document,description FROM journal_entries WHERE id=?")||sqlite3_bind_int64(q.p,1,id)!=SQLITE_OK||sqlite3_step(q.p)!=SQLITE_ROW)return false;auto date=txt(q.p,0),doc=txt(q.p,1),desc=txt(q.p,2);Stmt l;if(!prep(s->db,l,"SELECT account_id,debit,credit FROM journal_lines WHERE entry_id=? ORDER BY line_no")||sqlite3_bind_int64(l.p,1,id)!=SQLITE_OK)return false;std::vector<JournalLine> rows;int rc;while((rc=sqlite3_step(l.p))==SQLITE_ROW)rows.push_back({sqlite3_column_int(l.p,0),sqlite3_column_int64(l.p,1),sqlite3_column_int64(l.p,2)});if(rc!=SQLITE_DONE)return false;jnew(s);s->loading=true;s->id=id;s->rows=rows;SetWindowTextW(s->date,date.c_str());SetWindowTextW(s->doc,doc.c_str());SetWindowTextW(s->desc,desc.c_str());jdraw(s);s->loading=false;s->dirty=false;return true;}
bool jsave(Journal* s){auto date=get(s->date),doc=get(s->doc),desc=get(s->desc);sqlite3_int64 d,c;if(!dateok(date)||doc.find_first_not_of(L" \t")==std::wstring::npos||desc.find_first_not_of(L" \t")==std::wstring::npos||!jtot(s,d,c)||d!=c||d==0||s->rows.size()<2)return false; if(s->rows.size()==2 && s->rows[0].account==s->rows[1].account)return false;if(!exec(s->db,"BEGIN IMMEDIATE"))return false;sqlite3_int64 id=s->id;bool ok;{Stmt q;ok=prep(s->db,q,id?"UPDATE journal_entries SET date=?,document=?,description=? WHERE id=?":"INSERT INTO journal_entries(date,document,description) VALUES(?,?,?)")&&bind(q,1,date)&&bind(q,2,doc)&&bind(q,3,desc)&&(!id||sqlite3_bind_int64(q.p,4,id)==SQLITE_OK)&&sqlite3_step(q.p)==SQLITE_DONE;if(ok&&id)ok=sqlite3_changes(s->db.p)==1;if(ok&&!id)id=sqlite3_last_insert_rowid(s->db.p);}if(ok){Stmt q;ok=prep(s->db,q,"DELETE FROM journal_lines WHERE entry_id=?")&&sqlite3_bind_int64(q.p,1,id)==SQLITE_OK&&sqlite3_step(q.p)==SQLITE_DONE;}for(size_t i=0;i<s->rows.size()&&ok;++i){auto& r=s->rows[i];Stmt q;ok=prep(s->db,q,"INSERT INTO journal_lines(entry_id,line_no,account_id,debit,credit) VALUES(?,?,?,?,?)")&&sqlite3_bind_int64(q.p,1,id)==SQLITE_OK&&sqlite3_bind_int64(q.p,2,i)==SQLITE_OK&&sqlite3_bind_int(q.p,3,r.account)==SQLITE_OK&&sqlite3_bind_int64(q.p,4,r.debit)==SQLITE_OK&&sqlite3_bind_int64(q.p,5,r.credit)==SQLITE_OK&&sqlite3_step(q.p)==SQLITE_DONE;}if(!ok||!exec(s->db,"COMMIT")){exec(s->db,"ROLLBACK");return false;}s->id=id;s->dirty=false;return true;}
LRESULT CALLBACK jproc(HWND h,UINT m,WPARAM w,LPARAM l){Journal* s=(Journal*)GetWindowLongPtrW(h,GWLP_USERDATA);if(m==WM_NCCREATE){s=(Journal*)((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)s);}if(!s)return DefWindowProcW(h,m,w,l);switch(m){case WM_CREATE:{s->font=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,L"Tahoma");jl(h,s,L"Журнал — дата, документ, описание, сума",12,10,740);s->filter=je(h,s,12,34,530,20);jb(h,s,L"Търси",550,32,95,21);jb(h,s,L"Отвори",650,32,100,22);s->list=jc(h,s,L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|WS_TABSTOP|LBS_NOTIFY,12,68,738,130,23);jl(h,s,L"Дата (ГГГГ-ММ-ДД)",12,205,180);jl(h,s,L"Документ №",200,205,170);jl(h,s,L"Описание",380,205,360);s->date=je(h,s,12,228,175,30);s->doc=je(h,s,200,228,170,31);s->desc=je(h,s,380,228,370,32);jl(h,s,L"Сметка",12,260,350);jl(h,s,L"Дебит",380,260,105);jl(h,s,L"Кредит",500,260,105);s->account=jc(h,s,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,12,283,355,200,33);s->debit=je(h,s,380,283,110,34);s->credit=je(h,s,500,283,110,35);jb(h,s,L"Добави ред",620,280,130,36);s->lines=jc(h,s,L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|WS_TABSTOP|LBS_NOTIFY,12,318,738,125,37);jb(h,s,L"Премахни ред",12,450,145,38);s->total=jc(h,s,L"STATIC",L"",0,170,455,580,25);jb(h,s,L"Нова операция",12,490,155,39);jb(h,s,L"Запази",470,490,135,40);jb(h,s,L"Затвори",615,490,135,41);jnew(s);if(!jaccounts(s)||!jlist(s)){error(h,L"Грешка при зареждане на журнала.");return -1;} if(s->initialEntryId && !jload(s,s->initialEntryId)) error(h,L"Записът не може да се отвори."); return 0;}case WM_COMMAND:{int k=LOWORD(w);if(k>=30&&k<=35&&HIWORD(w)==EN_CHANGE&&!s->loading)s->dirty=true;switch(k){case 21:if(!jlist(s))error(h,L"Грешка при търсене.");return 0;case 23:if(HIWORD(w)!=LBN_DBLCLK)return 0; // fall through
case 22:{int i=(int)SendMessageW(s->list,LB_GETCURSEL,0,0);if(i>=0&&i<(int)s->ids.size()&&discard(h,s)&&!jload(s,s->ids[i]))error(h,L"Грешка при отваряне.");return 0;}case 36:{int i=(int)SendMessageW(s->account,CB_GETCURSEL,0,0);sqlite3_int64 d,c;if(i<0||i>=(int)s->accounts.size()||!s->accounts[i].active||!amount(get(s->debit),d)||!amount(get(s->credit),c)||((d>0)==(c>0))){error(h,L"Изберете активна сметка и положителна сума само по дебит или кредит.");return 0;}s->rows.push_back({s->accounts[i].id,d,c});s->dirty=true;jdraw(s);SetWindowTextW(s->debit,L"");SetWindowTextW(s->credit,L"");return 0;}case 38:{int i=(int)SendMessageW(s->lines,LB_GETCURSEL,0,0);if(i>=0&&i<(int)s->rows.size()){s->rows.erase(s->rows.begin()+i);s->dirty=true;jdraw(s);}return 0;}case 39:if(discard(h,s))jnew(s);return 0;case 40:if(!get(s->debit).empty()||!get(s->credit).empty()){error(h,L"Добавете въведения ред или изчистете сумите преди запис.");return 0;}if(!jsave(s))error(h,L"Записът е неуспешен. Проверете датата, документа, салдото и сметките.\nДебит и кредит на една и съща сметка без аналитична разлика са невалидни.");else{if(!jlist(s))error(h,L"Записано, но журналът не може да се обнови.");else MessageBoxW(h,L"Операцията е записана.",L"AME",MB_OK|MB_ICONINFORMATION);}return 0;case 41:SendMessageW(h,WM_CLOSE,0,0);return 0;}break;}case WM_CLOSE:if(discard(h,s))DestroyWindow(h);return 0;case WM_DESTROY:if(s->font)DeleteObject(s->font);return 0;}return DefWindowProcW(h,m,w,l);}
}
void ShowJournalWindow(HWND parent,int id,const std::wstring& name,std::int64_t initialEntryId){Journal s;s.initialEntryId=initialEntryId;if(!ledger(s.db,id)||!schema(s.db)){error(parent,L"Не може да се отвори журналът.");return;}WNDCLASSW wc={};wc.lpfnWndProc=jproc;wc.hInstance=GetModuleHandleW(NULL);wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.lpszClassName=L"AMEJournal005";RegisterClassW(&wc);auto title=L"Счетоводни операции — "+name;RECT r={0,0,765,535};AdjustWindowRectEx(&r,WS_CAPTION|WS_SYSMENU|WS_POPUP,FALSE,WS_EX_DLGMODALFRAME);HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,title.c_str(),WS_CAPTION|WS_SYSMENU|WS_POPUP,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,parent,NULL,wc.hInstance,&s);if(!h)return;EnableWindow(parent,FALSE);ShowWindow(h,SW_SHOW);MSG msg;while(IsWindow(h)){int n=GetMessageW(&msg,NULL,0,0);if(n<=0){if(n==0)PostQuitMessage((int)msg.wParam);break;}if(!IsDialogMessageW(h,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}if(IsWindow(h))DestroyWindow(h);EnableWindow(parent,TRUE);SetActiveWindow(parent);}

// Prototype modules share the existing company ledger.
namespace {
struct Proto { Db db; HFONT font=nullptr; HWND fields[13]={}, list=nullptr; int mode=0; int company=0; int autoVatBp=-1; std::wstring report; std::vector<sqlite3_int64> ids; };
bool pschema(Db& d){return schema(d)&&exec(d,"CREATE TABLE IF NOT EXISTS partners(id INTEGER PRIMARY KEY,name TEXT NOT NULL,eik TEXT NOT NULL,vat TEXT NOT NULL,address TEXT NOT NULL); CREATE TABLE IF NOT EXISTS invoices(id INTEGER PRIMARY KEY,partner INTEGER NOT NULL REFERENCES partners(id),direction INTEGER NOT NULL,date TEXT NOT NULL,number TEXT NOT NULL,item TEXT NOT NULL,quantity INTEGER NOT NULL,price INTEGER NOT NULL,net INTEGER NOT NULL,vat INTEGER NOT NULL,entry INTEGER NOT NULL UNIQUE REFERENCES journal_entries(id),UNIQUE(direction,partner,number)); CREATE TABLE IF NOT EXISTS invoice_tax_meta(invoice INTEGER PRIMARY KEY REFERENCES invoices(id),vat_bp INTEGER NOT NULL,legal_basis TEXT NOT NULL,method TEXT NOT NULL); CREATE TABLE IF NOT EXISTS payments(id INTEGER PRIMARY KEY,invoice INTEGER NOT NULL REFERENCES invoices(id),amount INTEGER NOT NULL,date TEXT NOT NULL,entry INTEGER NOT NULL UNIQUE REFERENCES journal_entries(id)); CREATE TABLE IF NOT EXISTS preferences(key TEXT PRIMARY KEY,value TEXT NOT NULL); CREATE TRIGGER IF NOT EXISTS protect_invoice_entry BEFORE UPDATE ON journal_entries WHEN EXISTS(SELECT 1 FROM invoices WHERE entry=OLD.id) OR EXISTS(SELECT 1 FROM payments WHERE entry=OLD.id) BEGIN SELECT RAISE(ABORT,'linked document'); END;");}
std::wstring pv(Proto* p,int i){return get(p->fields[i]);}
void pf(HWND h,Proto* p,int i,const wchar_t* labelText,int x,int y,int width=210){HWND a=CreateWindowW(L"STATIC",labelText,WS_CHILD|WS_VISIBLE,x,y,width,20,h,NULL,NULL,NULL);SendMessageW(a,WM_SETFONT,(WPARAM)p->font,TRUE);p->fields[i]=CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,x,y+21,width,25,h,NULL,NULL,NULL);SendMessageW(p->fields[i],WM_SETFONT,(WPARAM)p->font,TRUE);}
void pb(HWND h,Proto* p,const wchar_t* t,int id,int x,int y,int w=145){HWND a=CreateWindowW(L"BUTTON",t,WS_CHILD|WS_VISIBLE|WS_TABSTOP,x,y,w,30,h,(HMENU)(INT_PTR)id,NULL,NULL);SendMessageW(a,WM_SETFONT,(WPARAM)p->font,TRUE);}
void out(Proto* p,const std::wstring& t){p->report=t;SetWindowTextW(p->list,t.c_str());}
std::wstring signedmoney(sqlite3_int64 n){return n<0?L"-"+money(-n):money(n);}
int aid(Db& d,const std::wstring& code){Stmt q;if(!prep(d,q,"SELECT id FROM accounts WHERE code=? AND active=1")||!bind(q,1,code)||sqlite3_step(q.p)!=SQLITE_ROW)return 0;return sqlite3_column_int(q.p,0);}
bool post(Db& d,const std::wstring& date,const std::wstring& doc,const std::wstring& desc,const std::vector<JournalLine>& lines,sqlite3_int64& id){sqlite3_int64 a=0,b=0;for(auto& l:lines){if(!l.account||l.debit<0||l.credit<0||((l.debit>0)==(l.credit>0)))return false;a+=l.debit;b+=l.credit;}if(a!=b||!a)return false;{Stmt q;if(!prep(d,q,"INSERT INTO journal_entries(date,document,description) VALUES(?,?,?)")||!bind(q,1,date)||!bind(q,2,doc)||!bind(q,3,desc)||sqlite3_step(q.p)!=SQLITE_DONE)return false;id=sqlite3_last_insert_rowid(d.p);}int n=0;for(auto& l:lines){Stmt q;if(!prep(d,q,"INSERT INTO journal_lines VALUES(?,?,?,?,?)")||sqlite3_bind_int64(q.p,1,id)||sqlite3_bind_int(q.p,2,++n)||sqlite3_bind_int(q.p,3,l.account)||sqlite3_bind_int64(q.p,4,l.debit)||sqlite3_bind_int64(q.p,5,l.credit)||sqlite3_step(q.p)!=SQLITE_DONE)return false;}return true;}
void plist(Proto* p){Stmt q;std::wstring t=L"Контрагенти (ID | име | ЕИК)\r\n";if(!prep(p->db,q,"SELECT id,name,eik FROM partners ORDER BY name"))return;while(sqlite3_step(q.p)==SQLITE_ROW)t+=std::to_wstring(sqlite3_column_int(q.p,0))+L" | "+txt(q.p,1)+L" | "+txt(q.p,2)+L"\r\n";t+=L"\r\nФактури (ID | дата | номер | контрагент | общо | неплатено)\r\n";Stmt z;if(!prep(p->db,z,"SELECT i.id,i.date,i.number,p.name,i.net+i.vat,i.net+i.vat-COALESCE((SELECT SUM(amount) FROM payments WHERE invoice=i.id),0) FROM invoices i JOIN partners p ON p.id=i.partner ORDER BY i.id DESC"))return;while(sqlite3_step(z.p)==SQLITE_ROW)t+=std::to_wstring(sqlite3_column_int(z.p,0))+L" | "+txt(z.p,1)+L" | "+txt(z.p,2)+L" | "+txt(z.p,3)+L" | "+money(sqlite3_column_int64(z.p,4))+L" | "+money(sqlite3_column_int64(z.p,5))+L"\r\n";out(p,t);}
bool number(const std::wstring& s,int& n){if(s.empty()||s.size()>9||s.find_first_not_of(L"0123456789")!=std::wstring::npos)return false;n=_wtoi(s.c_str());return n>0;}
bool partner(Proto* p){Stmt q;return !pv(p,0).empty()&&prep(p->db,q,"INSERT INTO partners(name,eik,vat,address) VALUES(?,?,?,?)")&&bind(q,1,pv(p,0))&&bind(q,2,pv(p,1))&&bind(q,3,pv(p,2))&&bind(q,4,pv(p,3))&&sqlite3_step(q.p)==SQLITE_DONE;}
bool invoice(Proto* p,bool sale){
 int partnerId=0,qty=0;sqlite3_int64 price=0,vat=0;
 auto date=pv(p,5),doc=pv(p,6),item=pv(p,7),legal=pv(p,12);
 if(!number(pv(p,4),partnerId)||!number(pv(p,8),qty)||qty>100000||
    !amount(pv(p,9),price)||price<=0||price>9000000000000LL/qty||
    !amount(pv(p,10),vat)||!dateok(date)||doc.empty()||item.empty())return false;
 auto net=price*qty;
 if(vat>net)return false;
 if(vat==0&&legal.find_first_not_of(L" \t")==std::wstring::npos)return false;
 if(p->autoVatBp>=0){
  ame::Money expected=0,total=0;
  if(!ame::VatFromBase(net,p->autoVatBp,expected,total)||expected!=vat)return false;
 }
 int base=aid(p->db,pv(p,11)),party=aid(p->db,sale?L"411":L"401"),tax=vat?aid(p->db,sale?L"4532":L"4531"):0;
 if(!base||!party||(vat&&!tax))return false;
 std::vector<JournalLine> lines;
 if(sale){lines.push_back({party,net+vat,0});lines.push_back({base,0,net});if(vat)lines.push_back({tax,0,vat});}
 else{lines.push_back({base,net,0});if(vat)lines.push_back({tax,vat,0});lines.push_back({party,0,net+vat});}
 if(!exec(p->db,"BEGIN IMMEDIATE"))return false;
 sqlite3_int64 entry=0;bool ok=post(p->db,date,doc,item,lines,entry);
 if(ok){Stmt q;ok=prep(p->db,q,"INSERT INTO invoices(partner,direction,date,number,item,quantity,price,net,vat,entry) VALUES(?,?,?,?,?,?,?,?,?,?)")&&
  sqlite3_bind_int(q.p,1,partnerId)==SQLITE_OK&&sqlite3_bind_int(q.p,2,sale?1:0)==SQLITE_OK&&
  bind(q,3,date)&&bind(q,4,doc)&&bind(q,5,item)&&sqlite3_bind_int(q.p,6,qty)==SQLITE_OK&&
  sqlite3_bind_int64(q.p,7,price)==SQLITE_OK&&sqlite3_bind_int64(q.p,8,net)==SQLITE_OK&&
  sqlite3_bind_int64(q.p,9,vat)==SQLITE_OK&&sqlite3_bind_int64(q.p,10,entry)==SQLITE_OK&&
  sqlite3_step(q.p)==SQLITE_DONE;}
 if(ok){Stmt q;ok=prep(p->db,q,"INSERT INTO invoice_tax_meta(invoice,vat_bp,legal_basis,method) VALUES(?,?,?,?)")&&
  sqlite3_bind_int64(q.p,1,sqlite3_last_insert_rowid(p->db.p))==SQLITE_OK&&
  sqlite3_bind_int(q.p,2,p->autoVatBp)==SQLITE_OK&&bind(q,3,legal)&&
  bind(q,4,p->autoVatBp>=0?L"AME_AUTO_REVIEW":L"MANUAL_REVIEW")&&sqlite3_step(q.p)==SQLITE_DONE;}
 if(ok&&exec(p->db,"COMMIT")){p->autoVatBp=-1;return true;}
 exec(p->db,"ROLLBACK");return false;
}
void autoInvoiceVAT(HWND h,Proto* p){
 int qty=0;sqlite3_int64 price=0;
 if(!number(pv(p,8),qty)||!amount(pv(p,9),price)||price<=0||qty>100000||price>9000000000000LL/qty){
  MessageBoxW(h,L"Въведете валидни количество и единична цена.",L"AME",MB_OK|MB_ICONWARNING);return;
 }
 HMENU menu=CreatePopupMenu();
 AppendMenuW(menu,MF_STRING,4201,L"ДДС 20% — стандартен случай");
 AppendMenuW(menu,MF_STRING,4202,L"ДДС 9% — само когато законът позволява");
 AppendMenuW(menu,MF_STRING,4203,L"ДДС 0% — изисква правно основание");
 POINT pt={};GetCursorPos(&pt);
 UINT choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,pt.x,pt.y,0,h,nullptr);
 DestroyMenu(menu);
 if(choice<4201||choice>4203)return;
 int rate=choice==4201?2000:choice==4202?900:0;
 if(rate!=2000&&MessageBoxW(h,L"Избрана е ставка, която изисква специални условия или основание. Проверете по ЗДДС преди осчетоводяване.",L"AME — ДДС",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES)return;
 ame::Money tax=0,gross=0;
 if(!ame::VatFromBase(price*qty,rate,tax,gross)){MessageBoxW(h,L"Сумата е извън допустимия диапазон.",L"AME",MB_OK|MB_ICONERROR);return;}
 SetWindowTextW(p->fields[10],ame::FormatMoney(tax).c_str());p->autoVatBp=rate;
 if(rate==0)MessageBoxW(h,L"Преди запис попълнете правното основание за неначисляване в съответното поле.",L"AME — 0%",MB_OK|MB_ICONINFORMATION);
}
bool payment(Proto* p){int id;sqlite3_int64 v;if(!number(pv(p,0),id)||!amount(pv(p,1),v)||v<=0||!dateok(pv(p,2)))return false;int bank=aid(p->db,pv(p,3));if(!bank||!exec(p->db,"BEGIN IMMEDIATE"))return false;bool ok=false;int sale=0;std::wstring date;{Stmt q;if(prep(p->db,q,"SELECT direction,date,net+vat-COALESCE((SELECT SUM(amount) FROM payments WHERE invoice=invoices.id),0) FROM invoices WHERE id=?")&&!sqlite3_bind_int(q.p,1,id)&&sqlite3_step(q.p)==SQLITE_ROW){sale=sqlite3_column_int(q.p,0);date=txt(q.p,1);ok=v<=sqlite3_column_int64(q.p,2)&&pv(p,2)>=date;}}int party=aid(p->db,sale?L"411":L"401");sqlite3_int64 entry=0;if(ok)ok=post(p->db,pv(p,2),L"PAY-"+std::to_wstring(id),L"Плащане",sale?std::vector<JournalLine>{{bank,v,0},{party,0,v}}:std::vector<JournalLine>{{party,v,0},{bank,0,v}},entry);if(ok){Stmt q;ok=prep(p->db,q,"INSERT INTO payments(invoice,amount,date,entry) VALUES(?,?,?,?)")&&!sqlite3_bind_int(q.p,1,id)&&!sqlite3_bind_int64(q.p,2,v)&&bind(q,3,pv(p,2))&&!sqlite3_bind_int64(q.p,4,entry)&&sqlite3_step(q.p)==SQLITE_DONE;}if(ok&&exec(p->db,"COMMIT"))return true;exec(p->db,"ROLLBACK");return false;}
void report(Proto* p,bool detail){auto start=pv(p,0),end=pv(p,1);if(!dateok(start)||!dateok(end)||start>end){out(p,L"Невалиден период.");return;}Stmt q;std::wstring t;if(detail){if(!prep(p->db,q,"SELECT e.date,e.document,a.code,e.description,l.debit,l.credit FROM journal_lines l JOIN journal_entries e ON e.id=l.entry_id JOIN accounts a ON a.id=l.account_id WHERE e.date BETWEEN ? AND ? AND (?='' OR a.code=?) ORDER BY e.date,e.id,l.line_no")||!bind(q,1,start)||!bind(q,2,end)||!bind(q,3,pv(p,2))||!bind(q,4,pv(p,2)))return;t=L"Дата;Документ;Сметка;Описание;Дебит;Кредит\r\n";while(sqlite3_step(q.p)==SQLITE_ROW)t+=txt(q.p,0)+L";"+txt(q.p,1)+L";"+txt(q.p,2)+L";"+txt(q.p,3)+L";"+money(sqlite3_column_int64(q.p,4))+L";"+money(sqlite3_column_int64(q.p,5))+L"\r\n";}else{if(!prep(p->db,q,"SELECT a.code,a.name,a.kind,COALESCE((SELECT SUM(debit-credit) FROM opening WHERE account_id=a.id AND date=?1),0),COALESCE((SELECT SUM(l.debit) FROM journal_lines l JOIN journal_entries e ON e.id=l.entry_id WHERE l.account_id=a.id AND e.date BETWEEN ?1 AND ?2),0),COALESCE((SELECT SUM(l.credit) FROM journal_lines l JOIN journal_entries e ON e.id=l.entry_id WHERE l.account_id=a.id AND e.date BETWEEN ?1 AND ?2),0) FROM accounts a ORDER BY a.code")||!bind(q,1,start)||!bind(q,2,end))return;t=L"Сметка;Име;Начално нето;Оборот Д;Оборот К;Крайно нето\r\n";sqlite3_int64 income=0,expense=0;while(sqlite3_step(q.p)==SQLITE_ROW){auto o=sqlite3_column_int64(q.p,3),d=sqlite3_column_int64(q.p,4),c=sqlite3_column_int64(q.p,5);t+=txt(q.p,0)+L";"+txt(q.p,1)+L";"+signedmoney(o)+L";"+money(d)+L";"+money(c)+L";"+signedmoney(o+d-c)+L"\r\n";if(sqlite3_column_int(q.p,2)==3)income+=c-d;if(sqlite3_column_int(q.p,2)==4)expense+=d-c;}t+=L"\r\nПриходи: "+signedmoney(income)+L"\r\nРазходи: "+signedmoney(expense)+L"\r\nРезултат: "+signedmoney(income-expense);}out(p,t);}
bool exportText(Proto* p){auto path=pv(p, p->mode==1?3:0);if(path.empty())return false;HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return false;DWORD n;wchar_t bom=0xfeff;bool ok=WriteFile(f,&bom,2,&n,NULL)&&n==2;DWORD bytes=(DWORD)(p->report.size()*sizeof(wchar_t));ok=ok&&WriteFile(f,p->report.data(),bytes,&n,NULL)&&n==bytes;CloseHandle(f);return ok;}
bool backup(Proto* p,bool restore){auto path=pv(p,0);if(path.empty())return false;Db other;if(restore&&GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES)return false;if(!restore&&GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES)return false;if(!open(other,path))return false;if(restore){Stmt q;if(!prep(other,q,"SELECT count(*) FROM accounts")||sqlite3_step(q.p)!=SQLITE_ROW)return false;}sqlite3_backup* b=sqlite3_backup_init(restore?p->db.p:other.p,"main",restore?other.p:p->db.p,"main");if(!b)return false;int rc=sqlite3_backup_step(b,-1);int end=sqlite3_backup_finish(b);return rc==SQLITE_DONE&&end==SQLITE_OK;}
LRESULT CALLBACK pproc(HWND h,UINT m,WPARAM w,LPARAM l){Proto* p=(Proto*)GetWindowLongPtrW(h,GWLP_USERDATA);if(m==WM_NCCREATE){p=(Proto*)((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)p);}if(!p)return DefWindowProcW(h,m,w,l);switch(m){case WM_CREATE:{p->font=CreateFontW(16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Tahoma");int top=190;if(p->mode==0){pf(h,p,0,L"Име на контрагент",10,5);pf(h,p,1,L"ЕИК",230,5);pf(h,p,2,L"ДДС номер",450,5);pf(h,p,3,L"Адрес",10,57);pb(h,p,L"Добави контрагент",100,230,78,210);pf(h,p,4,L"ID контрагент",10,112);pf(h,p,5,L"Дата ГГГГ-ММ-ДД",230,112);pf(h,p,6,L"Номер фактура",450,112);pf(h,p,7,L"Стока / услуга",10,164);pf(h,p,8,L"Количество (цяло)",230,164);pf(h,p,9,L"Ед. цена",450,164);pf(h,p,10,L"ДДС сума",10,216);pf(h,p,11,L"Сметка приход/разход",230,216);pf(h,p,12,L"Основание за 0% / без ДДС",450,216);pb(h,p,L"Покупка",101,10,270);pb(h,p,L"Продажба",102,165,270);pb(h,p,L"Плащания",103,320,270);pb(h,p,L"Изчисли ДДС",111,472,270,190);top=310;}else if(p->mode==3){pf(h,p,0,L"ID фактура",10,5);pf(h,p,1,L"Сума",230,5);pf(h,p,2,L"Дата ГГГГ-ММ-ДД",450,5);pf(h,p,3,L"Сметка каса / банка",10,60);SetWindowTextW(p->fields[3],L"501");pb(h,p,L"Запиши плащане",104,230,80,210);top=125;}else if(p->mode==1){pf(h,p,0,L"От дата (начално салдо)",10,5);pf(h,p,1,L"До дата",230,5);pf(h,p,2,L"Сметка (за главна книга)",450,5);pf(h,p,3,L"Пълен път за CSV",10,60,430);pb(h,p,L"Оборотна ведомост",105,10,115,200);pb(h,p,L"Главна книга",106,220,115);pb(h,p,L"CSV експорт",107,375,115);top=155;}else{pf(h,p,0,L"Пълен път до архив .db",10,5,650);pb(h,p,L"Архивирай",108,10,60);pb(h,p,L"Възстанови",109,165,60);pf(h,p,1,L"Бележка за фирмата",10,105,430);pb(h,p,L"Запази настройка",110,450,126,200);top=170;Stmt q;if(prep(p->db,q,"SELECT value FROM preferences WHERE key='note'")&&sqlite3_step(q.p)==SQLITE_ROW)SetWindowTextW(p->fields[1],txt(q.p,0).c_str());}p->list=CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOHSCROLL|ES_AUTOVSCROLL,10,top,660,540-top,h,NULL,NULL,NULL);SendMessageW(p->list,WM_SETFONT,(WPARAM)p->font,TRUE);if(p->mode==0||p->mode==3)plist(p);return 0;}case WM_COMMAND:{bool ok=false;switch(LOWORD(w)){case 111:autoInvoiceVAT(h,p);return 0;case 100:ok=partner(p);break;case 101:case 102:if(MessageBoxW(h,L"Запис и осчетоводяване? Проверете сумите и сметките.",L"AME",MB_YESNO)!=IDYES)return 0;ok=invoice(p,LOWORD(w)==102);break;case 103:ShowPrototypeWindow(h,p->company,L"Плащания",3);plist(p);return 0;case 104:ok=payment(p);break;case 105:report(p,false);return 0;case 106:report(p,true);return 0;case 107:ok=exportText(p);break;case 108:ok=backup(p,false);break;case 109:if(MessageBoxW(h,L"ВНИМАНИЕ: заменя всички счетоводни данни на текущата фирма. Продължавате ли?",L"AME",MB_YESNO|MB_ICONWARNING)!=IDYES)return 0;ok=backup(p,true)&&pschema(p->db);break;case 110:{Stmt q;ok=prep(p->db,q,"INSERT OR REPLACE INTO preferences VALUES('note',?)")&&bind(q,1,pv(p,1))&&sqlite3_step(q.p)==SQLITE_DONE;break;}default:return 0;}MessageBoxW(h,ok?L"Готово.":L"Неуспешно. Проверете полетата, сметките и пътя.",L"AME",MB_OK|(ok?MB_ICONINFORMATION:MB_ICONERROR));if(ok&&(p->mode==0||p->mode==3))plist(p);return 0;}case WM_CLOSE:DestroyWindow(h);return 0;case WM_DESTROY:DeleteObject(p->font);return 0;}return DefWindowProcW(h,m,w,l);}
}
void ShowPrototypeWindow(HWND parent,int id,const std::wstring& name,int mode){Proto p;p.mode=mode;p.company=id;if(!ledger(p.db,id)||!pschema(p.db)){error(parent,L"Грешка в базата.");return;}WNDCLASSW wc={};wc.lpfnWndProc=pproc;wc.hInstance=GetModuleHandleW(NULL);wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.lpszClassName=L"AMEPrototype010";RegisterClassW(&wc);auto title=L"AME — "+name;HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,title.c_str(),WS_CAPTION|WS_SYSMENU|WS_POPUP,CW_USEDEFAULT,CW_USEDEFAULT,700,590,parent,NULL,wc.hInstance,&p);if(!h)return;EnableWindow(parent,FALSE);ShowWindow(h,SW_SHOW);MSG msg;while(IsWindow(h)){int n=GetMessageW(&msg,NULL,0,0);if(n<=0){if(!n)PostQuitMessage((int)msg.wParam);break;}if(!IsDialogMessageW(h,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}if(IsWindow(h))DestroyWindow(h);EnableWindow(parent,TRUE);SetActiveWindow(parent);}
