#include "extra_modules.h"
#include "../data/chart_template.h"
#include <windows.h>
#include <shlobj.h>
#include <sqlite3.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cwchar>

namespace {
struct Db {
    sqlite3* p=nullptr;
    ~Db(){if(p)sqlite3_close(p);}
};
struct Query {
    sqlite3_stmt* p=nullptr;
    ~Query(){if(p)sqlite3_finalize(p);}
};
bool execute(Db& db,const char* sql){return sqlite3_exec(db.p,sql,nullptr,nullptr,nullptr)==SQLITE_OK;}
bool prepare(Db& db,Query& q,const char* sql){return sqlite3_prepare_v2(db.p,sql,-1,&q.p,nullptr)==SQLITE_OK;}
bool bind(Query& q,int i,const std::wstring& s){return sqlite3_bind_text16(q.p,i,s.c_str(),-1,SQLITE_TRANSIENT)==SQLITE_OK;}
std::wstring text(sqlite3_stmt* p,int i){const void* v=sqlite3_column_text16(p,i);return v?static_cast<const wchar_t*>(v):L"";}
std::wstring read(HWND h){if(!h)return L"";int n=GetWindowTextLengthW(h);std::vector<wchar_t>b(n+1);GetWindowTextW(h,b.data(),n+1);return b.data();}
std::wstring amount(sqlite3_int64 v){bool negative=v<0;if(negative)v=-v;return (negative?L"-":L"")+std::to_wstring(v/100)+L"."+(v%100<10?L"0":L"")+std::to_wstring(v%100);}
std::wstring clean(std::wstring s){size_t a=s.find_first_not_of(L" \r\n\t");if(a==std::wstring::npos)return L"";size_t b=s.find_last_not_of(L" \r\n\t");return s.substr(a,b-a+1);}
bool digits(const std::wstring& s){return !s.empty()&&s.find_first_not_of(L"0123456789")==std::wstring::npos;}
bool dateValid(const std::wstring& s){if(s.size()!=10||s[4]!=L'-'||s[7]!=L'-')return false;std::wstring d=s.substr(0,4)+s.substr(5,2)+s.substr(8,2);if(!digits(d))return false;int y=_wtoi(s.substr(0,4).c_str()),m=_wtoi(s.substr(5,2).c_str()),day=_wtoi(s.substr(8,2).c_str());int days[]={31,28,31,30,31,30,31,31,30,31,30,31};if(y<1900||y>2200||m<1||m>12)return false;return day>=1&&day<=days[m-1]+(m==2&&y%4==0&&(y%100!=0||y%400==0));}
bool parseAmount(const std::wstring& v,sqlite3_int64& out){out=0;auto s=clean(v);if(s.empty())return false;size_t dot=s.find_first_of(L".,");std::wstring whole=dot==std::wstring::npos?s:s.substr(0,dot);std::wstring fraction=dot==std::wstring::npos?L"":s.substr(dot+1);if(!digits(whole)||fraction.size()>2||(!fraction.empty()&&!digits(fraction)))return false;if(whole.size()>13)return false;out=_wtoi64(whole.c_str())*100; if(fraction.size()==1)out+=10*(fraction[0]-L'0');else if(fraction.size()==2)out+=10*(fraction[0]-L'0')+fraction[1]-L'0';return out>=0&&out<900000000000000LL;}
std::wstring currentDate(){SYSTEMTIME t;GetLocalTime(&t);wchar_t b[32];swprintf_s(b,32,L"%04d-%02d-%02d",t.wYear,t.wMonth,t.wDay);return b;}
std::wstring periodKey(int y,int m){wchar_t b[32];swprintf_s(b,32,L"%04d-%02d",y,m);return b;}
std::wstring monthStart(int y,int m){return periodKey(y,m)+L"-01";}
std::wstring monthEnd(int y,int m){int days[]={31,28,31,30,31,30,31,31,30,31,30,31};int d=days[m-1]+(m==2&&y%4==0&&(y%100!=0||y%400==0));wchar_t b[20];swprintf_s(b,20,L"%04d-%02d-%02d",y,m,d);return b;}
std::wstring path(int id){wchar_t base[MAX_PATH]={};if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE,nullptr,SHGFP_TYPE_CURRENT,base)))return L"";std::wstring d=base;d+=L"\\AME";CreateDirectoryW(d.c_str(),nullptr);d+=L"\\companies";CreateDirectoryW(d.c_str(),nullptr);return d+L"\\company_"+std::to_wstring(id)+L".db";}
bool open(Db& db,int id){if(id<=0)return false;std::wstring file=path(id);if(file.empty()||sqlite3_open16(file.c_str(),&db.p)!=SQLITE_OK)return false;sqlite3_busy_timeout(db.p,5000);return EnsureChartTemplate(db.p)&&execute(db,"CREATE TABLE IF NOT EXISTS journal_entries(id INTEGER PRIMARY KEY,date TEXT NOT NULL,document TEXT NOT NULL,description TEXT NOT NULL);CREATE TABLE IF NOT EXISTS journal_lines(entry_id INTEGER NOT NULL,line_no INTEGER NOT NULL,account_id INTEGER NOT NULL,debit INTEGER NOT NULL,credit INTEGER NOT NULL,PRIMARY KEY(entry_id,line_no));CREATE TABLE IF NOT EXISTS opening(account_id INTEGER NOT NULL,date TEXT NOT NULL,debit INTEGER NOT NULL,credit INTEGER NOT NULL,PRIMARY KEY(account_id,date));CREATE TABLE IF NOT EXISTS partners(id INTEGER PRIMARY KEY,name TEXT NOT NULL,eik TEXT NOT NULL,vat TEXT NOT NULL,address TEXT NOT NULL);CREATE TABLE IF NOT EXISTS am_context(key TEXT PRIMARY KEY,value TEXT NOT NULL);CREATE TABLE IF NOT EXISTS am_directory(id INTEGER PRIMARY KEY,category INTEGER NOT NULL,code TEXT NOT NULL,name TEXT NOT NULL,note TEXT NOT NULL,UNIQUE(category,code));CREATE TABLE IF NOT EXISTS am_staff(id INTEGER PRIMARY KEY,name TEXT NOT NULL,identifier TEXT NOT NULL,position TEXT NOT NULL,start_date TEXT NOT NULL,note TEXT NOT NULL);CREATE TABLE IF NOT EXISTS am_assets(id INTEGER PRIMARY KEY,code TEXT NOT NULL UNIQUE,name TEXT NOT NULL,acquired TEXT NOT NULL,price INTEGER NOT NULL,life_months INTEGER NOT NULL,note TEXT NOT NULL);");}
std::wstring meta(Db& db,const wchar_t* key,const std::wstring& defaultValue){Query q;if(prepare(db,q,"SELECT value FROM am_context WHERE key=?")&&bind(q,1,key)&&sqlite3_step(q.p)==SQLITE_ROW)return text(q.p,0);return defaultValue;}
bool setMeta(Db& db,const wchar_t* key,const std::wstring& val){Query q;return prepare(db,q,"INSERT OR REPLACE INTO am_context(key,value) VALUES(?,?)")&&bind(q,1,key)&&bind(q,2,val)&&sqlite3_step(q.p)==SQLITE_DONE;}
void month(Db& db,int& year,int& mo){SYSTEMTIME t;GetLocalTime(&t);year=t.wYear;mo=t.wMonth;std::wstring v=meta(db,L"period",periodKey(year,mo));if(v.size()==7&&v[4]==L'-'&&digits(v.substr(0,4))&&digits(v.substr(5,2))){int y=_wtoi(v.substr(0,4).c_str()),m=_wtoi(v.substr(5,2).c_str());if(y>=1900&&y<=2200&&m>=1&&m<=12){year=y;mo=m;}}}

HWND control(HWND parent,HFONT font,const wchar_t* klass,const wchar_t* value,DWORD flags,int x,int y,int w,int h,int id=0){HWND c=CreateWindowExW(0,klass,value,WS_VISIBLE|WS_CHILD|flags,x,y,w,h,parent,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);if(c)SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);return c;}
HWND label(HWND h,HFONT f,const wchar_t* t,int x,int y,int w=180){return control(h,f,L"STATIC",t,0,x,y,w,22);}
HWND edit(HWND h,HFONT f,int x,int y,int w,int id=0){return control(h,f,L"EDIT",L"",ES_AUTOHSCROLL|WS_BORDER|WS_TABSTOP,x,y,w,27,id);}
void button(HWND h,HFONT f,const wchar_t* t,int x,int y,int w,int id){control(h,f,L"BUTTON",t,BS_PUSHBUTTON|WS_TABSTOP,x,y,w,31,id);}
void alert(HWND h,const std::wstring& s,bool ok){MessageBoxW(h,s.c_str(),L"AME",MB_OK|(ok?MB_ICONINFORMATION:MB_ICONERROR));}
struct AccountRow{int id,kind;std::wstring code,name;sqlite3_int64 opening=0,movDebit=0,movCredit=0,closing=0;};
struct State{
 Db db; HFONT font=nullptr;int mode=0,sub=0,company=0,selectedId=0;std::wstring companyName;
 HWND search=nullptr,results=nullptr,combo=nullptr,fields[6]={},list=nullptr,periodLabel=nullptr;
 std::vector<int> ids;std::wstring output;
};

bool sumMovements(Db& db,int account,const std::wstring& from,const std::wstring& to,sqlite3_int64& debit,sqlite3_int64& credit){debit=credit=0;Query q;return prepare(db,q,"SELECT COALESCE(SUM(l.debit),0),COALESCE(SUM(l.credit),0) FROM journal_lines l JOIN journal_entries e ON e.id=l.entry_id WHERE l.account_id=? AND e.date>=? AND e.date<=?")&&sqlite3_bind_int(q.p,1,account)==SQLITE_OK&&bind(q,2,from)&&bind(q,3,to)&&sqlite3_step(q.p)==SQLITE_ROW&&(debit=sqlite3_column_int64(q.p,0),credit=sqlite3_column_int64(q.p,1),true);}
// Latest effective opening snapshot, plus journal entries occurring after that
// snapshot and before the report start. This avoids the v0.1.0 exact-date bug.
bool accountBalance(Db& db,int id,const std::wstring& from,const std::wstring& to,AccountRow& a){
 Query q;
 std::wstring baseline=L"1900-01-01";
 sqlite3_int64 o=0;
 if(!prepare(db,q,"SELECT date,debit-credit FROM opening WHERE account_id=? AND date<=? ORDER BY date DESC LIMIT 1")||sqlite3_bind_int(q.p,1,id)!=SQLITE_OK||!bind(q,2,from))return false;
 int rc=sqlite3_step(q.p);
 if(rc==SQLITE_ROW){baseline=text(q.p,0);o=sqlite3_column_int64(q.p,1);}else if(rc!=SQLITE_DONE)return false;
 // The opening snapshot represents the balance at the beginning of its date.
 // A journal entry on the reporting start date belongs in movements, not opening.
 Query prior;
 if(!prepare(db,prior,"SELECT COALESCE(SUM(l.debit),0),COALESCE(SUM(l.credit),0) FROM journal_lines l JOIN journal_entries e ON e.id=l.entry_id WHERE l.account_id=? AND e.date>=? AND e.date<?")||sqlite3_bind_int(prior.p,1,id)!=SQLITE_OK||!bind(prior,2,baseline)||!bind(prior,3,from)||sqlite3_step(prior.p)!=SQLITE_ROW)return false;
 a.opening=o+sqlite3_column_int64(prior.p,0)-sqlite3_column_int64(prior.p,1);
 if(!sumMovements(db,id,from,to,a.movDebit,a.movCredit))return false;
 a.closing=a.opening+a.movDebit-a.movCredit;
 return true;
}
std::vector<AccountRow> allAccounts(Db& db,const std::wstring& start,const std::wstring& end){std::vector<AccountRow> rows;Query q;if(!prepare(db,q,"SELECT id,code,name,kind FROM accounts ORDER BY code"))return rows;while(sqlite3_step(q.p)==SQLITE_ROW){AccountRow a; a.id=sqlite3_column_int(q.p,0);a.code=text(q.p,1);a.name=text(q.p,2);a.kind=sqlite3_column_int(q.p,3);if(accountBalance(db,a.id,start,end,a))rows.push_back(a);}return rows;}
std::wstring csvSafe(std::wstring s){for(auto& c:s)if(c==L';'||c==L'\r'||c==L'\n')c=L' ';return s;}

// Reports are accounting workpapers, NOT Bulgarian statutory forms or НАП files.
std::wstring buildReport(State* s,int kind,const std::wstring& start,const std::wstring& end,const std::wstring& acct){
 auto rows=allAccounts(s->db,start,end);
 std::wstring out;
 if(kind==0||kind==5){ // chronological document list, optionally by date/doc
  out=L"Дата;Документ;Описание;Сметка;Дебит;Кредит\r\n";
  Query q;
  const char* sql="SELECT e.date,e.document,e.description,a.code,l.debit,l.credit FROM journal_entries e JOIN journal_lines l ON l.entry_id=e.id JOIN accounts a ON a.id=l.account_id WHERE e.date BETWEEN ? AND ? AND (?='' OR a.code=?) ORDER BY e.date,e.id,l.line_no";
  if(!prepare(s->db,q,sql)||!bind(q,1,start)||!bind(q,2,end)||!bind(q,3,acct)||!bind(q,4,acct))return L"Грешка при генериране на хронология.";
  while(sqlite3_step(q.p)==SQLITE_ROW)out+=csvSafe(text(q.p,0))+L";"+csvSafe(text(q.p,1))+L";"+csvSafe(text(q.p,2))+L";"+text(q.p,3)+L";"+amount(sqlite3_column_int64(q.p,4))+L";"+amount(sqlite3_column_int64(q.p,5))+L"\r\n";
 } else if(kind==3||kind==6||kind==7){
  // Monthly movement by account, as opposed to the trial balance.
  out=L"Месец;Сметка;Наименование;Дебит;Кредит;Нетен оборот\r\n";
  Query q;
  if(!prepare(s->db,q,"SELECT substr(e.date,1,7),a.code,a.name,COALESCE(SUM(l.debit),0),COALESCE(SUM(l.credit),0) FROM journal_lines l JOIN journal_entries e ON e.id=l.entry_id JOIN accounts a ON a.id=l.account_id WHERE e.date BETWEEN ? AND ? AND (?='' OR a.code=?) GROUP BY substr(e.date,1,7),a.id ORDER BY substr(e.date,1,7),a.code")||!bind(q,1,start)||!bind(q,2,end)||!bind(q,3,acct)||!bind(q,4,acct))return L"Грешка в месечната справка.";
  while(sqlite3_step(q.p)==SQLITE_ROW){auto d=sqlite3_column_int64(q.p,3),c=sqlite3_column_int64(q.p,4);out+=text(q.p,0)+L";"+text(q.p,1)+L";"+csvSafe(text(q.p,2))+L";"+amount(d)+L";"+amount(c)+L";"+amount(d-c)+L"\r\n";}
 }else if(kind==4||kind==8||kind==9||kind==11||kind==13){
  // Account-level journal and cash book. Exact line data is preserved.
  out=L"Дата;Документ;Сметка;Наименование;Описание;Дебит;Кредит\r\n";
  Query q;
  const char* sql="SELECT e.date,e.document,a.code,a.name,e.description,l.debit,l.credit FROM journal_lines l JOIN journal_entries e ON e.id=l.entry_id JOIN accounts a ON a.id=l.account_id WHERE e.date BETWEEN ? AND ? AND (?='' OR a.code=?) ORDER BY a.code,e.date,e.id,l.line_no";
  if(!prepare(s->db,q,sql)||!bind(q,1,start)||!bind(q,2,end)||!bind(q,3,acct)||!bind(q,4,acct))return L"Грешка в журнала.";
  while(sqlite3_step(q.p)==SQLITE_ROW){auto code=text(q.p,2);if(kind==13 && code!=L"501"&&code!=L"502"&&code!=L"503"&&code!=L"504")continue;out+=text(q.p,0)+L";"+csvSafe(text(q.p,1))+L";"+code+L";"+csvSafe(text(q.p,3))+L";"+csvSafe(text(q.p,4))+L";"+amount(sqlite3_column_int64(q.p,5))+L";"+amount(sqlite3_column_int64(q.p,6))+L"\r\n";}
 }else if(kind==1||kind==2||kind==14){
  // Trial balance and account statements derived from the same journal.
  out=L"Сметка;Наименование;Начално дебит;Начално кредит;Оборот дебит;Оборот кредит;Крайно дебит;Крайно кредит\r\n";
  sqlite3_int64 openingD=0,openingC=0,d=0,c=0,closingD=0,closingC=0;
  for(auto& a:rows){
   if(!acct.empty()&&a.code!=acct)continue;
      if(!a.opening&&!a.movDebit&&!a.movCredit&&!a.closing)continue;
   sqlite3_int64 od=std::max((sqlite3_int64)0,a.opening),oc=std::max((sqlite3_int64)0,-a.opening),cd=std::max((sqlite3_int64)0,a.closing),cc=std::max((sqlite3_int64)0,-a.closing);
   out+=a.code+L";"+csvSafe(a.name)+L";"+amount(od)+L";"+amount(oc)+L";"+amount(a.movDebit)+L";"+amount(a.movCredit)+L";"+amount(cd)+L";"+amount(cc)+L"\r\n";
   openingD+=od;openingC+=oc;d+=a.movDebit;c+=a.movCredit;closingD+=cd;closingC+=cc;
  }
  out+=L"ОБЩО;;"+amount(openingD)+L";"+amount(openingC)+L";"+amount(d)+L";"+amount(c)+L";"+amount(closingD)+L";"+amount(closingC)+L"\r\n";
 }else if(kind==10){
  out=L"Кореспонденция за двуредови записи;Дебит;Кредит;Сума\r\n";
  Query q;
  if(!prepare(s->db,q,"SELECT e.document,a1.code,a2.code,l1.debit FROM journal_entries e JOIN journal_lines l1 ON l1.entry_id=e.id AND l1.debit>0 JOIN journal_lines l2 ON l2.entry_id=e.id AND l2.credit>0 JOIN accounts a1 ON a1.id=l1.account_id JOIN accounts a2 ON a2.id=l2.account_id WHERE e.date BETWEEN ? AND ? AND (SELECT COUNT(*) FROM journal_lines x WHERE x.entry_id=e.id)=2 ORDER BY e.date,e.id")||!bind(q,1,start)||!bind(q,2,end))return L"Грешка в справката за кореспонденции.";
  while(sqlite3_step(q.p)==SQLITE_ROW)out+=csvSafe(text(q.p,0))+L";"+text(q.p,1)+L";"+text(q.p,2)+L";"+amount(sqlite3_column_int64(q.p,3))+L"\r\n";
  out+=L"Само операции с точно един дебитен и един кредитен ред.\r\n";
 }else if(kind==15||kind==16){
  if(kind==16){sqlite3_int64 incomes=0,expenses=0;out=L"Група;Сметка;Наименование;Оборот без салдо\r\n";for(auto& a:rows){if(a.kind!=3&&a.kind!=4)continue;sqlite3_int64 n=a.kind==3?a.movCredit-a.movDebit:a.movDebit-a.movCredit;if(!n)continue;out+=(a.kind==3?L"Приход":L"Разход")+std::wstring(L";")+a.code+L";"+csvSafe(a.name)+L";"+amount(n)+L"\r\n";if(a.kind==3)incomes+=n;else expenses+=n;}out+=L"Общо приходи;;;"+amount(incomes)+L"\r\nОбщо разходи;;;"+amount(expenses)+L"\r\nСчетоводен резултат;;;"+amount(incomes-expenses)+L"\r\n";}
  else {out=L"Сметка;Наименование;Крайно нето\r\n";for(auto& a:rows){if(a.kind==3||a.kind==4||!a.closing)continue;out+=a.code+L";"+csvSafe(a.name)+L";"+amount(a.closing)+L"\r\n";}out+=L"\r\nИнформационна справка - НЕ е официален счетоводен баланс.\r\n";}
 }else if(kind==18||kind==19){
  out=L"Дата;Фактура;Контрагент;ЕИК;Данъчна основа;ДДС\r\n";
  Query q;const char* sql="SELECT i.date,i.number,p.name,p.eik,i.net,i.vat FROM invoices i JOIN partners p ON p.id=i.partner WHERE i.direction=? AND i.date BETWEEN ? AND ? ORDER BY i.date,i.id";
  // Earlier databases may not have invoice tables at all.
  if(!prepare(s->db,q,sql))return L"Няма въведени фактури в текущата фирма.\r\n";
  if(sqlite3_bind_int(q.p,1,kind==19)!=SQLITE_OK||!bind(q,2,start)||!bind(q,3,end))return L"Грешка при справка.";
  while(sqlite3_step(q.p)==SQLITE_ROW)out+=csvSafe(text(q.p,0))+L";"+csvSafe(text(q.p,1))+L";"+csvSafe(text(q.p,2))+L";"+csvSafe(text(q.p,3))+L";"+amount(sqlite3_column_int64(q.p,4))+L";"+amount(sqlite3_column_int64(q.p,5))+L"\r\n";
  out+=L"\r\nВЪТРЕШНА СПРАВКА, НЕ Е ДНЕВНИК ВЪВ ФОРМАТА НА НАП.\r\n";
 }else if(kind>=22&&kind<=25){
  out=kind!=23?L"Инв. №;Актив;Дата;Стойност;Срок (месеци)\r\n":L"Инв. №;Актив;Амортизация на месец;Срок (месеци);Пълна стойност\r\n";
  Query q;
  if(!prepare(s->db,q,"SELECT code,name,acquired,price,life_months FROM am_assets ORDER BY code"))return L"Грешка в инвентарната книга.";
  while(sqlite3_step(q.p)==SQLITE_ROW){auto code=text(q.p,0),name=csvSafe(text(q.p,1)),acquired=text(q.p,2);const sqlite3_int64 price=sqlite3_column_int64(q.p,3);const int life=sqlite3_column_int(q.p,4);
   if((kind==24 && (acquired<start || acquired>end)) || (kind==25 && acquired>end))continue;
   if(kind!=23)out+=code+L";"+name+L";"+acquired+L";"+amount(price)+L";"+std::to_wstring(life)+L"\r\n";
   else out+=code+L";"+name+L";"+amount(life?price/life:0)+L";"+std::to_wstring(life)+L";"+amount(price)+L"\r\n";
  }
  if(kind==23)out+=L"\r\nРаботен линейно изчислен план. Не е данъчен амортизационен план.\r\n";
 }else if(kind==17){
  out=L"Парични движения по счетоводни сметки 501,502,503,504\r\n";
  for(auto& a:rows)if(a.code==L"501"||a.code==L"502"||a.code==L"503"||a.code==L"504")out+=a.code+L" "+a.name+L": "+amount(a.movDebit-a.movCredit)+L"\r\n";
 }else return L"Тази нормативна справка не е реализирана и НЕ трябва да се използва за подаване към НАП.";
 return out;
}

const wchar_t* reportNames[]={
 L"Хронологичен опис на операциите",L"Оборотна ведомост месечна",L"Оборотна ведомост с натрупване",L"Справка по месеци за сметка",L"Журнали на сметките по дати",L"Журнали на сметките по документи",L"Извлечение от сметка обобщено месечно",L"Извлечение от сметка с натрупване",L"Извлечение от сметка подробно месечно",L"Извлечение от сметка подробно с натрупване",L"Кореспонденция по дебита и кредита",L"Справка по дати по сметка",L"Валутен дневник",L"Касова книга",L"Главна книга",L"Баланс (работен)",L"Отчет за приходи и разходи (работен)",L"Паричен поток (работен)",L"Дневник на покупките (вътрешен)",L"Дневник на продажбите (вътрешен)",L"Декларация по ДДС (неподдържана)",L"Файлове за ДДС към НАП (неподдържани)",L"Инвентарна книга",L"Амортизационен план — работен",L"Придобити активи за период",L"Придобити активи с натрупване"
};
const int REPORT_COUNT=(int)(sizeof(reportNames)/sizeof(reportNames[0]));

bool writeCSV(State* s,const std::wstring& f){if(f.empty()||s->output.empty())return false;HANDLE file=CreateFileW(f.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;DWORD written=0;const wchar_t mark=0xfeff;bool ok=WriteFile(file,&mark,2,&written,nullptr)&&written==2;DWORD bytes=(DWORD)(s->output.size()*2);ok=ok&&WriteFile(file,s->output.c_str(),bytes,&written,nullptr)&&written==bytes;CloseHandle(file);if(!ok)DeleteFileW(f.c_str());return ok;}
void renderReport(State* s){int kind=(int)SendMessageW(s->combo,CB_GETCURSEL,0,0);if(kind<0||kind>=REPORT_COUNT)kind=0;auto start=read(s->fields[0]),end=read(s->fields[1]),acct=clean(read(s->fields[2]));if(!dateValid(start)||!dateValid(end)||start>end){alert(s->results,L"Невалидни дати. Използвайте ГГГГ-ММ-ДД.",false);return;}if(kind==2||kind==7||kind==9||kind==14){start=start.substr(0,4)+L"-01-01";}s->output=buildReport(s,kind,start,end,acct);SetWindowTextW(s->results,s->output.c_str());}

void fillDirectory(State* s){s->ids.clear();SendMessageW(s->list,LB_RESETCONTENT,0,0);Query q;const char* sql=s->sub==0?"SELECT id,name,eik,vat,address FROM partners ORDER BY name":s->sub==2?"SELECT id,name,identifier,position,start_date,note FROM am_staff ORDER BY name":s->sub==3?"SELECT id,code,name,acquired,price,life_months,note FROM am_assets ORDER BY code":"SELECT id,code,name,note FROM am_directory WHERE category=? ORDER BY code";if(!prepare(s->db,q,sql))return;if(s->sub!=0&&s->sub!=2&&s->sub!=3)sqlite3_bind_int(q.p,1,s->sub);while(sqlite3_step(q.p)==SQLITE_ROW){int id=sqlite3_column_int(q.p,0);s->ids.push_back(id);std::wstring name=s->sub==0||s->sub==2?text(q.p,1):text(q.p,1)+L"  "+text(q.p,2);if(s->sub==3)name+=L"  |  "+amount(sqlite3_column_int64(q.p,4))+L" лв/евро (според отчетна валута)";SendMessageW(s->list,LB_ADDSTRING,0,(LPARAM)name.c_str());}}
void clearFields(State* s){s->selectedId=0;for(auto h:s->fields)if(h)SetWindowTextW(h,L"");SendMessageW(s->list,LB_SETCURSEL,-1,0);}
void selectDirectory(State* s){int at=(int)SendMessageW(s->list,LB_GETCURSEL,0,0);if(at<0||at>=(int)s->ids.size())return;s->selectedId=s->ids[at];Query q;const char* sql=s->sub==0?"SELECT name,eik,vat,address FROM partners WHERE id=?":s->sub==2?"SELECT name,identifier,position,start_date,note FROM am_staff WHERE id=?":s->sub==3?"SELECT code,name,acquired,price,life_months,note FROM am_assets WHERE id=?":"SELECT code,name,note FROM am_directory WHERE id=?";if(!prepare(s->db,q,sql)||sqlite3_bind_int(q.p,1,s->selectedId)!=SQLITE_OK||sqlite3_step(q.p)!=SQLITE_ROW)return;int n=s->sub==0?4:s->sub==2?5:s->sub==3?6:3;for(int i=0;i<n;++i){std::wstring v=(s->sub==3&&i==3)?amount(sqlite3_column_int64(q.p,i)):(s->sub==3&&i==4)?std::to_wstring(sqlite3_column_int(q.p,i)):text(q.p,i);SetWindowTextW(s->fields[i],v.c_str());}}
bool saveDirectory(State* s){
 std::wstring f[6];for(int i=0;i<6;++i)f[i]=clean(read(s->fields[i]));
 if(f[0].empty()||(s->sub==3&&(!dateValid(f[2])||f[1].empty())))return false;
 Query q;bool updated=s->selectedId>0;const char* sql=nullptr;
 if(s->sub==0)sql=updated?"UPDATE partners SET name=?,eik=?,vat=?,address=? WHERE id=?":"INSERT INTO partners(name,eik,vat,address) VALUES(?,?,?,?)";
 else if(s->sub==2)sql=updated?"UPDATE am_staff SET name=?,identifier=?,position=?,start_date=?,note=? WHERE id=?":"INSERT INTO am_staff(name,identifier,position,start_date,note) VALUES(?,?,?,?,?)";
 else if(s->sub==3)sql=updated?"UPDATE am_assets SET code=?,name=?,acquired=?,price=?,life_months=?,note=? WHERE id=?":"INSERT INTO am_assets(code,name,acquired,price,life_months,note) VALUES(?,?,?,?,?,?)";
 else sql=updated?"UPDATE am_directory SET code=?,name=?,note=? WHERE id=? AND category=?":"INSERT INTO am_directory(code,name,note,category) VALUES(?,?,?,?)";
 if(!prepare(s->db,q,sql))return false;
 if(s->sub==3){sqlite3_int64 price;int months=_wtoi(f[4].c_str());if(!parseAmount(f[3],price)||months<1||months>1200)return false;if(!bind(q,1,f[0])||!bind(q,2,f[1])||!bind(q,3,f[2])||sqlite3_bind_int64(q.p,4,price)!=SQLITE_OK||sqlite3_bind_int(q.p,5,months)!=SQLITE_OK||!bind(q,6,f[5]))return false;}
 else {int count=s->sub==0?4:s->sub==2?5:3;for(int i=0;i<count;++i)if(!bind(q,i+1,f[i]))return false;}
 int index=s->sub==3?7:s->sub==0?5:s->sub==2?6:4;
 if(updated){if(sqlite3_bind_int(q.p,index,s->selectedId)!=SQLITE_OK)return false;if(s->sub!=0&&s->sub!=2&&s->sub!=3 && sqlite3_bind_int(q.p,index+1,s->sub)!=SQLITE_OK)return false;}
 else if(s->sub!=0&&s->sub!=2&&s->sub!=3 && sqlite3_bind_int(q.p,4,s->sub)!=SQLITE_OK)return false;
 return sqlite3_step(q.p)==SQLITE_DONE;
}
void showDirectoryHeader(HWND h,State* s){const wchar_t* captions[]={L"Номенклатура на контрагентите",L"Номенклатура на разходите",L"Библиотека на персонала",L"Инвентарна книга на дълготрайните активи",L"Номенклатура на материалите",L"Номенклатура на складовете",L"Библиотека на длъжностите",L"Библиотека на начисленията",L"Библиотека на удръжките"};label(h,s->font,captions[s->sub],12,12,675);s->list=control(h,s->font,L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|LBS_NOTIFY,12,50,700,215,100);
 const wchar_t* title[9][6]={
 {L"Име на контрагент",L"ЕИК",L"ДДС номер",L"Адрес",L"",L""},
 {L"Код",L"Разход",L"Бележка",L"",L"",L""},
 {L"Три имена",L"Идентификатор (ЕГН/ЛНЧ)",L"Длъжност",L"Дата на назначаване",L"Бележки",L""},
 {L"Инвентарен №",L"Актив",L"Дата на придобиване",L"Отчетна стойност",L"Срок (месеци)",L"Бележка"},
 {L"Код",L"Материал",L"Бележка",L"",L"",L""},
 {L"Код",L"Склад",L"Бележка",L"",L"",L""},
 {L"Код",L"Длъжност",L"Бележка",L"",L"",L""},
 {L"Код",L"Вид начисление",L"Бележка",L"",L"",L""},
 {L"Код",L"Вид удръжка",L"Бележка",L"",L"",L""}
 };
 int n=s->sub==0?4:s->sub==2?5:s->sub==3?6:3;
 for(int i=0;i<n;++i){int x=12+(i%3)*238;int y=280+(i/3)*66;label(h,s->font,title[s->sub][i],x,y,228);s->fields[i]=edit(h,s->font,x,y+23,225,110+i);}
 button(h,s->font,L"Нов",12,443,125,201);button(h,s->font,L"Запази",155,443,130,202);button(h,s->font,L"Затвори",582,443,130,203);
 label(h,s->font,L"Съхранява се отделно за избраната фирма.",305,449,280);
 fillDirectory(s);
}
void showReportHeader(HWND h,State* s){label(h,s->font,L"Справки — по реалните счетоводни записи на фирмата",12,10,680);s->combo=control(h,s->font,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,12,38,710,450,100);for(int i=0;i<REPORT_COUNT;++i)SendMessageW(s->combo,CB_ADDSTRING,0,(LPARAM)reportNames[i]);SendMessageW(s->combo,CB_SETCURSEL,s->sub>=0&&s->sub<REPORT_COUNT?s->sub:1,0);int y,m;month(s->db,y,m);label(h,s->font,L"От дата",12,77,120);label(h,s->font,L"До дата",180,77,120);label(h,s->font,L"Сметка (по избор)",348,77,170);s->fields[0]=edit(h,s->font,12,101,155);s->fields[1]=edit(h,s->font,180,101,155);s->fields[2]=edit(h,s->font,348,101,190);SetWindowTextW(s->fields[0],monthStart(y,m).c_str());SetWindowTextW(s->fields[1],monthEnd(y,m).c_str());button(h,s->font,L"Генерирай",548,100,174,201);s->results=control(h,s->font,L"EDIT",L"",WS_BORDER|WS_HSCROLL|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_AUTOHSCROLL|ES_READONLY,12,142,710,340,202);label(h,s->font,L"CSV път:",12,494,120);s->fields[3]=edit(h,s->font,90,491,452);button(h,s->font,L"Експорт CSV",548,490,174,203);renderReport(s);}
void showPeriodHeader(HWND h,State* s){int y,m;month(s->db,y,m);label(h,s->font,L"Текущ счетоводен период",20,25,440);label(h,s->font,L"Година",20,74,160);label(h,s->font,L"Месец (1–12)",215,74,160);s->fields[0]=edit(h,s->font,20,101,160);s->fields[1]=edit(h,s->font,215,101,160);SetWindowTextW(s->fields[0],std::to_wstring(y).c_str());SetWindowTextW(s->fields[1],std::to_wstring(m).c_str());label(h,s->font,L"Избраният период се използва като начална дата в справките.",20,151,620);label(h,s->font,L"Не затваря счетоводни периоди и не променя вече въведени записи.",20,182,670);button(h,s->font,L"Запази периода",20,250,230,201);button(h,s->font,L"Затвори",270,250,180,203);}

LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){State* s=(State*)GetWindowLongPtrW(h,GWLP_USERDATA);if(msg==WM_NCCREATE){s=(State*)((CREATESTRUCTW*)lp)->lpCreateParams;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)s);}if(!s)return DefWindowProcW(h,msg,wp,lp);switch(msg){case WM_CREATE:
 s->font=CreateFontW(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Tahoma");
 if(s->mode==0)showReportHeader(h,s);else if(s->mode==1)showPeriodHeader(h,s);else showDirectoryHeader(h,s);
 return 0;
 case WM_COMMAND:{int id=LOWORD(wp),event=HIWORD(wp);if(s->mode==0){if(id==201){renderReport(s);return 0;}if(id==203){if(writeCSV(s,clean(read(s->fields[3]))))alert(h,L"Експортът е записан.",true);else alert(h,L"Неуспешен експорт. Използвайте нов пълен път до CSV файл.",false);return 0;}if(id==100&&event==CBN_SELCHANGE){renderReport(s);return 0;}}
 else if(s->mode==1){if(id==201){auto yy=clean(read(s->fields[0])),mm=clean(read(s->fields[1]));int y=_wtoi(yy.c_str()),m=_wtoi(mm.c_str());if(!digits(yy)||!digits(mm)||y<1900||y>2200||m<1||m>12||!setMeta(s->db,L"period",periodKey(y,m)))alert(h,L"Невалидна година/месец или грешка при запис.",false);else alert(h,L"Периодът е записан.",true);return 0;}}
 else {if(id==100&&event==LBN_SELCHANGE){selectDirectory(s);return 0;}if(id==201){clearFields(s);return 0;}if(id==202){if(saveDirectory(s)){fillDirectory(s);clearFields(s);alert(h,L"Записано успешно.",true);}else alert(h,L"Записът е неуспешен. Проверете задължителните полета или вече използвания код.",false);return 0;}}
 if(id==203){DestroyWindow(h);return 0;}return 0;}
 case WM_CLOSE:DestroyWindow(h);return 0;
 case WM_DESTROY:if(s->font)DeleteObject(s->font);return 0;
 }return DefWindowProcW(h,msg,wp,lp);}
void show(HWND parent,int id,const std::wstring& company,int mode,int sub){State s;s.company=id;s.companyName=company;s.mode=mode;s.sub=sub;if(!open(s.db,id)){alert(parent,L"Не може да се отвори базата на фирмата.",false);return;}static const wchar_t* klass=L"AMEExtendedWorkspace";WNDCLASSW wc={};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.lpszClassName=klass;RegisterClassW(&wc);std::wstring title=L"AME — "+company;int w=755,ht=mode==1?350:590;HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,klass,title.c_str(),WS_CAPTION|WS_SYSMENU|WS_POPUP,CW_USEDEFAULT,CW_USEDEFAULT,w,ht,parent,nullptr,wc.hInstance,&s);if(!h)return;EnableWindow(parent,FALSE);ShowWindow(h,SW_SHOW);MSG msg;while(IsWindow(h)){BOOL n=GetMessageW(&msg,nullptr,0,0);if(n<=0){if(!n)PostQuitMessage((int)msg.wParam);break;}if(!IsDialogMessageW(h,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}if(IsWindow(h))DestroyWindow(h);EnableWindow(parent,TRUE);SetActiveWindow(parent);}
} // namespace

void ShowAMEReport(HWND parent,int id,const std::wstring& company,int index){show(parent,id,company,0,index);}
void ShowAMEPeriod(HWND parent,int id,const std::wstring& company){show(parent,id,company,1,0);}
void ShowAMEDirectory(HWND parent,int id,const std::wstring& company,int type){if(type<0||type>8)return;show(parent,id,company,2,type);}
