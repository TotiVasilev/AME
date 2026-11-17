#include "compliance_workbench.h"
#include "../data/chart_template.h"
#include <windows.h>
#include <shlobj.h>
#include <commdlg.h>
#include <sqlite3.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cwchar>
#include <cstdint>
#include <limits>
#include <utility>

namespace {
struct Db { sqlite3* p=nullptr; ~Db(){if(p)sqlite3_close(p);} };
struct Query { sqlite3_stmt* p=nullptr; ~Query(){if(p)sqlite3_finalize(p);} };
bool exec(Db& d,const char* q){return sqlite3_exec(d.p,q,nullptr,nullptr,nullptr)==SQLITE_OK;}
bool prep(Db& d,Query& q,const char* s){return sqlite3_prepare_v2(d.p,s,-1,&q.p,nullptr)==SQLITE_OK;}
bool textParam(Query& q,int i,const std::wstring& v){return sqlite3_bind_text16(q.p,i,v.c_str(),-1,SQLITE_TRANSIENT)==SQLITE_OK;}
bool numParam(Query& q,int i,sqlite3_int64 v){return sqlite3_bind_int64(q.p,i,v)==SQLITE_OK;}
std::wstring col(sqlite3_stmt* q,int n){auto p=sqlite3_column_text16(q,n);return p?static_cast<const wchar_t*>(p):L"";}
std::wstring get(HWND h){if(!h)return L"";int len=GetWindowTextLengthW(h);std::vector<wchar_t> b(len+1);GetWindowTextW(h,b.data(),len+1);return b.data();}
bool digits(const std::wstring& a){return !a.empty()&&a.find_first_not_of(L"0123456789")==std::wstring::npos;}
bool month(const std::wstring& s){return s.size()==7&&s[4]==L'-'&&digits(s.substr(0,4))&&digits(s.substr(5,2))&&_wtoi(s.substr(0,4).c_str())>=2000&&_wtoi(s.substr(0,4).c_str())<=2200&&_wtoi(s.substr(5,2).c_str())>=1&&_wtoi(s.substr(5,2).c_str())<=12;}
bool day(const std::wstring& s){if(s.size()!=10||s[4]!=L'-'||s[7]!=L'-'||!digits(s.substr(0,4)+s.substr(5,2)+s.substr(8,2)))return false;int y=_wtoi(s.substr(0,4).c_str()),m=_wtoi(s.substr(5,2).c_str()),d=_wtoi(s.substr(8,2).c_str());if(y<2000||y>2200||m<1||m>12)return false;int days[]={31,28,31,30,31,30,31,31,30,31,30,31};return d>0&&d<=days[m-1]+(m==2&&(y%4==0&&(y%100!=0||y%400==0)));}
std::wstring now(){SYSTEMTIME t;GetLocalTime(&t);wchar_t b[32];swprintf_s(b,32,L"%04u-%02u-%02u",t.wYear,t.wMonth,t.wDay);return b;}
std::wstring curMonth(){return now().substr(0,7);}
std::wstring fmt(sqlite3_int64 a){bool neg=a<0;if(neg)a=-a;return(neg?L"-":L"")+std::to_wstring(a/100)+L"."+(a%100<10?L"0":L"")+std::to_wstring(a%100);}
bool money(const std::wstring& s,sqlite3_int64& n){n=0;bool neg=false;size_t start=0;if(!s.empty()&&s[0]==L'-'){neg=true;start=1;}size_t dot=s.find_first_of(L".,",start);std::wstring a=s.substr(start,dot==std::wstring::npos?std::wstring::npos:dot-start);std::wstring b=dot==std::wstring::npos?L"":s.substr(dot+1);if(!digits(a)||a.size()>11||b.size()>2||(!b.empty()&&!digits(b)))return false;n=_wtoi64(a.c_str())*100;if(b.size()==1)n+=10*(b[0]-L'0');if(b.size()==2)n+=10*(b[0]-L'0')+(b[1]-L'0');if(neg)n=-n;return true;}
bool basisPoints(const std::wstring& s,int& bps){sqlite3_int64 cents=0;if(!money(s,cents)||cents<0||cents>10000)return false;bps=(int)cents;return true;}
sqlite3_int64 mulBps(sqlite3_int64 value,int bps){return (value*bps+5000)/10000;}
std::wstring escapeXml(const std::wstring& s){std::wstring o;for(wchar_t c:s){switch(c){case L'&':o+=L"&amp;";break;case L'<':o+=L"&lt;";break;case L'>':o+=L"&gt;";break;case L'\"':o+=L"&quot;";break;case L'\'':o+=L"&apos;";break;default: if(c>=32&&c!=127)o+=c;}}return o;}
std::wstring csv(const std::wstring& s){std::wstring o=L"\"";for(wchar_t c:s){if(c==L'\"')o+=L"\"\"";else if(c==L'\r'||c==L'\n')o+=L' ';else o+=c;}o+=L'\"';return o;}

bool isLocked(Db& db,int year){Query q;return prep(db,q,"SELECT 1 FROM am_fiscal_year WHERE year=? AND locked=1")&&numParam(q,1,year)&&sqlite3_step(q.p)==SQLITE_ROW;}
// All workbench tables are additive. The database triggers protect closed years
// even from OTHER AME windows; there is no user-interface-only enforcement.
bool makeSchema(Db& db){
 const char* schema=
 "PRAGMA foreign_keys=ON;"
 "CREATE TABLE IF NOT EXISTS journal_entries(id INTEGER PRIMARY KEY,date TEXT NOT NULL,document TEXT NOT NULL,description TEXT NOT NULL);"
 "CREATE TABLE IF NOT EXISTS journal_lines(entry_id INTEGER NOT NULL,line_no INTEGER NOT NULL,account_id INTEGER NOT NULL,debit INTEGER NOT NULL,credit INTEGER NOT NULL,PRIMARY KEY(entry_id,line_no));"
 "CREATE TABLE IF NOT EXISTS opening(account_id INTEGER NOT NULL,date TEXT NOT NULL,debit INTEGER NOT NULL,credit INTEGER NOT NULL,PRIMARY KEY(account_id,date));"
 "CREATE TABLE IF NOT EXISTS partners(id INTEGER PRIMARY KEY,name TEXT NOT NULL,eik TEXT NOT NULL,vat TEXT NOT NULL,address TEXT NOT NULL);"
 "CREATE TABLE IF NOT EXISTS invoices(id INTEGER PRIMARY KEY,partner INTEGER NOT NULL REFERENCES partners(id),direction INTEGER NOT NULL,date TEXT NOT NULL,number TEXT NOT NULL,item TEXT NOT NULL,quantity INTEGER NOT NULL,price INTEGER NOT NULL,net INTEGER NOT NULL,vat INTEGER NOT NULL,entry INTEGER NOT NULL UNIQUE REFERENCES journal_entries(id),UNIQUE(direction,partner,number));"
 "CREATE TABLE IF NOT EXISTS payments(id INTEGER PRIMARY KEY,invoice INTEGER NOT NULL REFERENCES invoices(id),amount INTEGER NOT NULL,date TEXT NOT NULL,entry INTEGER NOT NULL UNIQUE REFERENCES journal_entries(id));"
 "CREATE TABLE IF NOT EXISTS am_staff(id INTEGER PRIMARY KEY,name TEXT NOT NULL,identifier TEXT NOT NULL,position TEXT NOT NULL,start_date TEXT NOT NULL,note TEXT NOT NULL);"
 "CREATE TABLE IF NOT EXISTS am_assets(id INTEGER PRIMARY KEY,code TEXT NOT NULL UNIQUE,name TEXT NOT NULL,acquired TEXT NOT NULL,price INTEGER NOT NULL,life_months INTEGER NOT NULL,note TEXT NOT NULL);"
 "CREATE TABLE IF NOT EXISTS am_vat_work(id INTEGER PRIMARY KEY,period TEXT NOT NULL,direction INTEGER NOT NULL,kind TEXT NOT NULL,number TEXT NOT NULL,date TEXT NOT NULL,partner TEXT NOT NULL,vat_id TEXT NOT NULL,base INTEGER NOT NULL,vat INTEGER NOT NULL,treatment TEXT NOT NULL,currency TEXT NOT NULL,note TEXT NOT NULL,created TEXT NOT NULL,UNIQUE(period,direction,number,vat_id));"
 "CREATE TABLE IF NOT EXISTS am_pay_work(id INTEGER PRIMARY KEY,period TEXT NOT NULL,staff INTEGER NOT NULL,gross INTEGER NOT NULL,employee_ins INTEGER NOT NULL,employer_ins INTEGER NOT NULL,tax INTEGER NOT NULL,other INTEGER NOT NULL,note TEXT NOT NULL,UNIQUE(period,staff));"
 "CREATE TABLE IF NOT EXISTS am_employment_work(id INTEGER PRIMARY KEY,staff INTEGER NOT NULL,kind INTEGER NOT NULL,docno TEXT NOT NULL,docdate TEXT NOT NULL,effective TEXT NOT NULL,basis TEXT NOT NULL,terms TEXT NOT NULL,salary INTEGER NOT NULL,note TEXT NOT NULL,UNIQUE(kind,docno));"
 "CREATE TABLE IF NOT EXISTS am_depr_work(id INTEGER PRIMARY KEY,asset INTEGER NOT NULL,period TEXT NOT NULL,accounting INTEGER NOT NULL,tax INTEGER NOT NULL,note TEXT NOT NULL,UNIQUE(asset,period));"
 "CREATE TABLE IF NOT EXISTS am_fiscal_year(year INTEGER PRIMARY KEY,locked INTEGER NOT NULL DEFAULT 0,entry INTEGER,closed_at TEXT NOT NULL DEFAULT '',reviewer TEXT NOT NULL DEFAULT '',note TEXT NOT NULL DEFAULT '');"
 "CREATE TABLE IF NOT EXISTS am_pay_rates(period TEXT PRIMARY KEY,employee_bp INTEGER NOT NULL,employer_bp INTEGER NOT NULL,tax_bp INTEGER NOT NULL,minimum INTEGER NOT NULL,maximum INTEGER NOT NULL,approved_by TEXT NOT NULL,created_at TEXT NOT NULL);"
 "CREATE TABLE IF NOT EXISTS am_pay_calc_audit(id INTEGER PRIMARY KEY,period TEXT NOT NULL,at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,reviewer TEXT NOT NULL,employees INTEGER NOT NULL,profile TEXT NOT NULL);"
 "CREATE TABLE IF NOT EXISTS am_pay_fund_profile(period TEXT NOT NULL,staff INTEGER NOT NULL,insurance_base INTEGER NOT NULL,tax_relief INTEGER NOT NULL,tax_bp INTEGER NOT NULL,reviewer TEXT NOT NULL,insured_type TEXT NOT NULL,worked_days INTEGER NOT NULL,nkpd TEXT NOT NULL,kid TEXT NOT NULL,updated_at TEXT NOT NULL,PRIMARY KEY(period,staff));"
 "CREATE TABLE IF NOT EXISTS am_pay_fund_rates(period TEXT NOT NULL,staff INTEGER NOT NULL,fund INTEGER NOT NULL,employee_bp INTEGER NOT NULL,employer_bp INTEGER NOT NULL,PRIMARY KEY(period,staff,fund));"
 "CREATE TABLE IF NOT EXISTS am_pay_fund_result(period TEXT NOT NULL,staff INTEGER NOT NULL,fund INTEGER NOT NULL,employee_amount INTEGER NOT NULL,employer_amount INTEGER NOT NULL,calculated_at TEXT NOT NULL,PRIMARY KEY(period,staff,fund));"
 "CREATE TABLE IF NOT EXISTS am_tax_assets(asset INTEGER PRIMARY KEY,category INTEGER NOT NULL CHECK(category BETWEEN 1 AND 7),basis INTEGER NOT NULL CHECK(basis>=0),rate_bp INTEGER NOT NULL,service_date TEXT NOT NULL,reviewer TEXT NOT NULL);"
 "CREATE TABLE IF NOT EXISTS am_compliance_audit(id INTEGER PRIMARY KEY,at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,action TEXT NOT NULL,detail TEXT NOT NULL);";
 if(!exec(db,schema))return false;
 // Prevent writes to closed years; no triggers on am_fiscal_year itself so
 // the close transaction can enter the closing journal before locking it.
 const char* locks=
 "CREATE TRIGGER IF NOT EXISTS ame_lock_je_insert BEFORE INSERT ON journal_entries WHEN EXISTS(SELECT 1 FROM am_fiscal_year WHERE year=CAST(substr(NEW.date,1,4) AS INTEGER) AND locked=1) BEGIN SELECT RAISE(ABORT,'AME: accounting year locked'); END;"
 "CREATE TRIGGER IF NOT EXISTS ame_lock_je_update BEFORE UPDATE ON journal_entries WHEN EXISTS(SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year IN (CAST(substr(OLD.date,1,4) AS INTEGER),CAST(substr(NEW.date,1,4) AS INTEGER))) BEGIN SELECT RAISE(ABORT,'AME: accounting year locked'); END;"
 "CREATE TRIGGER IF NOT EXISTS ame_lock_je_delete BEFORE DELETE ON journal_entries WHEN EXISTS(SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year=CAST(substr(OLD.date,1,4) AS INTEGER)) BEGIN SELECT RAISE(ABORT,'AME: accounting year locked'); END;"
 "CREATE TRIGGER IF NOT EXISTS ame_lock_jl_insert BEFORE INSERT ON journal_lines WHEN EXISTS(SELECT 1 FROM journal_entries e JOIN am_fiscal_year y ON y.year=CAST(substr(e.date,1,4) AS INTEGER) WHERE e.id=NEW.entry_id AND y.locked=1) BEGIN SELECT RAISE(ABORT,'AME: accounting year locked'); END;"
 "CREATE TRIGGER IF NOT EXISTS ame_lock_jl_update BEFORE UPDATE ON journal_lines WHEN EXISTS(SELECT 1 FROM journal_entries e JOIN am_fiscal_year y ON y.year=CAST(substr(e.date,1,4) AS INTEGER) WHERE (e.id=OLD.entry_id OR e.id=NEW.entry_id) AND y.locked=1) BEGIN SELECT RAISE(ABORT,'AME: accounting year locked'); END;"
 "CREATE TRIGGER IF NOT EXISTS ame_lock_jl_delete BEFORE DELETE ON journal_lines WHEN EXISTS(SELECT 1 FROM journal_entries e JOIN am_fiscal_year y ON y.year=CAST(substr(e.date,1,4) AS INTEGER) WHERE e.id=OLD.entry_id AND y.locked=1) BEGIN SELECT RAISE(ABORT,'AME: accounting year locked'); END;"
 "CREATE TRIGGER IF NOT EXISTS ame_lock_open_insert BEFORE INSERT ON opening WHEN EXISTS(SELECT 1 FROM am_fiscal_year WHERE year=CAST(substr(NEW.date,1,4) AS INTEGER) AND locked=1) BEGIN SELECT RAISE(ABORT,'AME: accounting year locked'); END;"
 "CREATE TRIGGER IF NOT EXISTS ame_lock_open_update BEFORE UPDATE ON opening WHEN EXISTS(SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year IN (CAST(substr(OLD.date,1,4) AS INTEGER),CAST(substr(NEW.date,1,4) AS INTEGER))) BEGIN SELECT RAISE(ABORT,'AME: accounting year locked'); END;"
 "CREATE TRIGGER IF NOT EXISTS ame_lock_open_delete BEFORE DELETE ON opening WHEN EXISTS(SELECT 1 FROM am_fiscal_year WHERE year=CAST(substr(OLD.date,1,4) AS INTEGER) AND locked=1) BEGIN SELECT RAISE(ABORT,'AME: accounting year locked'); END;";
 if(!exec(db,locks))return false;
 // Other modules use either period YYYY-MM or YYYY-MM-DD.
 struct T {const char* table;const char* field;};
 const T t[]={ {"invoices","date"},{"payments","date"},{"am_vat_work","period"},{"am_pay_work","period"},{"am_depr_work","period"},{"am_employment_work","docdate"},{"am_pay_rates","period"} };
 for(const auto& x:t){
  for(int op=0;op<3;++op){const char* operation=op==0?"INSERT":op==1?"UPDATE":"DELETE";const char* row=op==2?"OLD":"NEW";
   std::string name="ame_lock_"+std::string(x.table)+"_"+operation;
   std::string trigger="CREATE TRIGGER IF NOT EXISTS "+name+" BEFORE "+operation+" ON "+x.table+" WHEN EXISTS(SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year=CAST(substr("+row+"."+x.field+",1,4) AS INTEGER)) ";
   if(op==1)trigger+="OR EXISTS(SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year=CAST(substr(OLD."+std::string(x.field)+",1,4) AS INTEGER)) ";
   trigger+="BEGIN SELECT RAISE(ABORT,'AME: accounting year locked'); END;";
   if(!exec(db,trigger.c_str()))return false;
  }
 }
 return true;
}

bool openCompany(Db& db,int id){if(id<=0)return false;wchar_t base[MAX_PATH]={};if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE,nullptr,SHGFP_TYPE_CURRENT,base)))return false;std::wstring root=std::wstring(base)+L"\\AME";CreateDirectoryW(root.c_str(),nullptr);root+=L"\\companies";CreateDirectoryW(root.c_str(),nullptr);std::wstring f=root+L"\\company_"+std::to_wstring(id)+L".db";
 if(sqlite3_open16(f.c_str(),&db.p)!=SQLITE_OK)return false;sqlite3_busy_timeout(db.p,5000);return EnsureChartTemplate(db.p)&&makeSchema(db);}

struct State { Db db; int company=0,mode=0; std::wstring companyName,preview; HWND hwnd=nullptr,edit[9]={},combo=nullptr,list=nullptr,out=nullptr; HFONT font=nullptr;std::vector<int> ids;};
void say(HWND h,const std::wstring& s,bool success=false){MessageBoxW(h,s.c_str(),L"AME — професионални модули",MB_OK|(success?MB_ICONINFORMATION:MB_ICONWARNING));}
HWND control(HWND parent,HFONT font,const wchar_t* cls,const wchar_t* value,DWORD styles,int x,int y,int w,int h,int id){HWND c=CreateWindowExW(0,cls,value,WS_VISIBLE|WS_CHILD|styles,x,y,w,h,parent,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);if(c)SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);return c;}
void label(HWND h,State* s,const wchar_t* name,int x,int y,int width=220){control(h,s->font,L"STATIC",name,0,x,y,width,22,0);}
void field(HWND h,State* s,int index,const wchar_t* name,int x,int y,int w=250,const wchar_t* defaultValue=L""){label(h,s,name,x,y);s->edit[index]=control(h,s->font,L"EDIT",defaultValue,ES_AUTOHSCROLL|WS_BORDER|WS_TABSTOP,x,y+24,w,27,201+index);}
void btn(HWND h,State* s,const wchar_t* name,int id,int x,int y,int w=175){control(h,s->font,L"BUTTON",name,BS_PUSHBUTTON|WS_TABSTOP,x,y,w,35,id);}
void setOut(State* s,const std::wstring& v){s->preview=v;SetWindowTextW(s->out,v.c_str());}
std::wstring val(State* s,int i){return get(s->edit[i]);}
std::wstring selectedMonth(State* s){return val(s,0);}
std::wstring selectedYear(State* s){return val(s,0);}

bool saveUtf8(HWND parent,const std::wstring& payload,const wchar_t* caption){
 wchar_t filename[MAX_PATH]={};OPENFILENAMEW ofn={};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=parent;ofn.lpstrFile=filename;ofn.nMaxFile=MAX_PATH;ofn.lpstrFilter=L"Текстови файлове (*.txt;*.csv;*.xml)\0*.txt;*.csv;*.xml\0Всички файлове\0*.*\0\0";ofn.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;ofn.lpstrTitle=caption;if(!GetSaveFileNameW(&ofn))return false;
 int size=WideCharToMultiByte(CP_UTF8,0,payload.data(),(int)payload.size(),nullptr,0,nullptr,nullptr);if(size<=0)return false;std::vector<char> buffer(size);if(WideCharToMultiByte(CP_UTF8,0,payload.data(),(int)payload.size(),buffer.data(),size,nullptr,nullptr)!=size)return false;
 HANDLE f=CreateFileW(filename,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return false;const unsigned char bom[]={0xEF,0xBB,0xBF};DWORD n=0;bool ok=WriteFile(f,bom,3,&n,nullptr)&&n==3&&WriteFile(f,buffer.data(),(DWORD)buffer.size(),&n,nullptr)&&n==buffer.size();CloseHandle(f);return ok;
}

std::wstring vatReview(State* s){std::wstring p=selectedMonth(s);if(!month(p))return L"Невалиден месец. Формат ГГГГ-ММ.";
 std::wstring out=L"ДДС / НАП — ВЪТРЕШНА ПРОВЕРКА ЗА "+p+L"\r\nНе е официална справка-декларация или файл за подаване.\r\n\r\n";
 Query q;if(!prep(s->db,q,"SELECT direction,kind,number,date,partner,vat_id,base,vat,treatment,currency FROM am_vat_work WHERE period=? ORDER BY direction,date,id")||!textParam(q,1,p))return out+L"Грешка при отваряне на регистъра.";
 sqlite3_int64 base[2]={},tax[2]={};int rows=0,issues=0;
 while(sqlite3_step(q.p)==SQLITE_ROW){int d=sqlite3_column_int(q.p,0);if(d!=0&&d!=1)continue;++rows;auto no=col(q.p,2),date=col(q.p,3),who=col(q.p,4),vatId=col(q.p,5),treat=col(q.p,8),curr=col(q.p,9);auto b=sqlite3_column_int64(q.p,6),t=sqlite3_column_int64(q.p,7);
  bool valid=day(date)&&date.substr(0,7)==p&&!no.empty()&&!who.empty()&&!treat.empty()&&curr==L"EUR"&&b>=0&&t>=0;
  if(!valid)++issues;else{base[d]+=b;tax[d]+=t;}
  out+=std::wstring(valid?L"OK  ":L"!!  ")+(d?L"Продажба ":L"Покупка ")+no+L" / "+date+L" / "+who+L" / "+vatId+L" / "+treat+L" / "+fmt(b)+L" + "+fmt(t)+L" "+curr+L"\r\n";
 }
 // Existing posted invoices must also be classified, or monthly totals are incomplete.
 int uncategorized=0;Query other;if(prep(s->db,other,"SELECT COUNT(*) FROM invoices WHERE date>=? AND date<?")&&textParam(other,1,p+L"-01")&&textParam(other,2,p+L"-32")&&sqlite3_step(other.p)==SQLITE_ROW)uncategorized=sqlite3_column_int(other.p,0);
 out+=L"\r\nВалидирани покупки: "+fmt(base[0])+L" / ДДС: "+fmt(tax[0])+L"\r\nВалидирани продажби: "+fmt(base[1])+L" / ДДС: "+fmt(tax[1])+L"\r\n";
 out+=L"Записи: "+std::to_wstring(rows)+L"; невалидни/непълни: "+std::to_wstring(issues)+L"; стари фактури без НАП класификация: "+std::to_wstring(uncategorized)+L"\r\n";
 if(uncategorized||issues)out+=L"БЛОКИРАНО: регистърът не е пълен; не трябва да се подава.\r\n";
 out+=L"\r\nОфициалните ППЗДДС прил.12 колони, кодирането CP1251 и деклар. форми за 2026 не се генерират от тези непълни данни.\r\n";
 return out;
}

bool loadRates(State* s,int& emp,int& employer,int& tax,sqlite3_int64& low,sqlite3_int64& high){
 auto p=selectedMonth(s);Query q;if(!prep(s->db,q,"SELECT employee_bp,employer_bp,tax_bp,minimum,maximum FROM am_pay_rates WHERE period=?")||!textParam(q,1,p)||sqlite3_step(q.p)!=SQLITE_ROW)return false;
 emp=sqlite3_column_int(q.p,0);employer=sqlite3_column_int(q.p,1);tax=sqlite3_column_int(q.p,2);low=sqlite3_column_int64(q.p,3);high=sqlite3_column_int64(q.p,4);return true;
}
std::wstring payrollPreview(State* s){std::wstring p=selectedMonth(s);if(!month(p))return L"Въведете период ГГГГ-ММ.";
 int ep,er,tax;sqlite3_int64 low,high;if(!loadRates(s,ep,er,tax,low,high))return L"Първо запазете процентите и основите. Няма автоматично заредени 2026 осигурителни ставки.";
 std::wstring o=L"РАЗЧЕТ ПО НАСТРОЕНИ ПРОЦЕНТИ — "+p+L"\r\nНЕ Е ЗАКОНОВА ВЕДОМОСТ И НЕ Е Д1/Д6.\r\n\r\n";
 Query q;if(!prep(s->db,q,"SELECT s.name,p.gross,p.other,p.employee_ins,p.employer_ins,p.tax FROM am_pay_work p JOIN am_staff s ON s.id=p.staff WHERE p.period=? ORDER BY s.name")||!textParam(q,1,p))return L"Грешка при ведомостта.";
 int count=0;while(sqlite3_step(q.p)==SQLITE_ROW){auto gross=sqlite3_column_int64(q.p,1),other=sqlite3_column_int64(q.p,2);auto base=std::min(high,std::max(low,gross)),employee=mulBps(base,ep),employer=mulBps(base,er),taxable=std::max<sqlite3_int64>(0,gross-employee),withheld=mulBps(taxable,tax),net=gross-employee-withheld-other;
 o+=col(q.p,0)+L": брутно "+fmt(gross)+L", осиг. основа "+fmt(base)+L", лични "+fmt(employee)+L", работодател "+fmt(employer)+L", ДОД "+fmt(withheld)+L", нето "+fmt(net)+L"\r\n";++count;
 }
 o+=L"\r\nСлужители: "+std::to_wstring(count)+L". Приложете само след проверка от ТРЗ специалист.\r\n";
 return o;
}

bool saveRates(State* s,std::wstring& error){auto p=selectedMonth(s);int ep,er,tax;sqlite3_int64 low,high;
 if(!month(p)||!basisPoints(val(s,1),ep)||!basisPoints(val(s,2),er)||!basisPoints(val(s,3),tax)||!money(val(s,4),low)||!money(val(s,5),high)||low<0||high<low||high==0||val(s,6).empty()){
  error=L"Въведете валиден месец, ставки (%), граници и име на проверяващия.";return false;}
 if(isLocked(s->db,_wtoi(p.substr(0,4).c_str()))){error=L"Годината е заключена.";return false;}
 Query q;return prep(s->db,q,"INSERT OR REPLACE INTO am_pay_rates(period,employee_bp,employer_bp,tax_bp,minimum,maximum,approved_by,created_at) VALUES(?,?,?,?,?,?,?,datetime('now'))")&&textParam(q,1,p)&&numParam(q,2,ep)&&numParam(q,3,er)&&numParam(q,4,tax)&&numParam(q,5,low)&&numParam(q,6,high)&&textParam(q,7,val(s,6))&&sqlite3_step(q.p)==SQLITE_DONE;
}

bool applyRates(State* s,std::wstring& error){auto p=selectedMonth(s);int ep,er,tax;sqlite3_int64 low,high;if(!month(p)||!loadRates(s,ep,er,tax,low,high)){error=L"Липсва профил на проценти.";return false;}
 if(isLocked(s->db,_wtoi(p.substr(0,4).c_str()))){error=L"Заключена финансова година.";return false;}
 if(!exec(s->db,"BEGIN IMMEDIATE"))return false;
 bool ok=true;int n=0;Query select;
 ok=prep(s->db,select,"SELECT id,gross,other FROM am_pay_work WHERE period=?")&&textParam(select,1,p);
 if(ok){while(sqlite3_step(select.p)==SQLITE_ROW){auto id=sqlite3_column_int64(select.p,0),gross=sqlite3_column_int64(select.p,1),other=sqlite3_column_int64(select.p,2);auto base=std::min(high,std::max(low,gross)),employee=mulBps(base,ep),employer=mulBps(base,er),withheld=mulBps(std::max<sqlite3_int64>(0,gross-employee),tax);
    if(employee+withheld+other>gross){error=L"Изчислените удръжки надвишават брутната сума.";ok=false;break;}
    Query update;if(!prep(s->db,update,"UPDATE am_pay_work SET employee_ins=?,employer_ins=?,tax=? WHERE id=?")||!numParam(update,1,employee)||!numParam(update,2,employer)||!numParam(update,3,withheld)||!numParam(update,4,id)||sqlite3_step(update.p)!=SQLITE_DONE){ok=false;break;}++n;
  }}
 if(ok&&n){Query q;ok=prep(s->db,q,"INSERT INTO am_pay_calc_audit(period,reviewer,employees,profile) VALUES(?,?,?,?)")&&textParam(q,1,p)&&textParam(q,2,val(s,6))&&numParam(q,3,n)&&textParam(q,4,L"Проверен от потребителя, въведени ставки; НЕ законова автоматизация")&&sqlite3_step(q.p)==SQLITE_DONE;}
 if(ok&&n&&exec(s->db,"COMMIT"))return true;exec(s->db,"ROLLBACK");if(n==0)error=L"Няма въведени заплати за този месец.";else if(error.empty())error=L"Проблем при актуализацията.";return false;
}

std::wstring d1d6Preview(State* s){auto p=selectedMonth(s);if(!month(p))return L"Невалиден период.";
 std::wstring o=L"Д1 / Д6 — ПРЕДВАРИТЕЛНА КОНТРОЛНА ВЕДОМОСТ, НЕ ФАЙЛ ЗА НАП\r\nМесец: "+p+L"\r\n\r\n";
 // Select the saved per-fund result and employee category as well as the payroll
 // row. Missing profiles are reported explicitly, never treated as 0% rates.
 const char* sql=
  "SELECT s.name,s.identifier,p.gross,p.employee_ins,p.employer_ins,p.tax,"
  " f.insured_type,f.worked_days,f.nkpd,f.kid,f.reviewer,"
  " (SELECT COUNT(*) FROM am_pay_fund_result r WHERE r.staff=p.staff AND r.period=p.period),"
  " COALESCE((SELECT SUM(r.employee_amount) FROM am_pay_fund_result r WHERE r.staff=p.staff AND r.period=p.period),0),"
  " COALESCE((SELECT SUM(r.employer_amount) FROM am_pay_fund_result r WHERE r.staff=p.staff AND r.period=p.period),0)"
  " FROM am_pay_work p JOIN am_staff s ON s.id=p.staff"
  " LEFT JOIN am_pay_fund_profile f ON f.staff=p.staff AND f.period=p.period"
  " WHERE p.period=? ORDER BY s.name";
 Query q;if(!prep(s->db,q,sql)||!textParam(q,1,p))return o+L"Грешка при четене.";
 sqlite3_int64 gross=0,employee=0,employer=0,tax=0;int count=0,issues=0;
 while(sqlite3_step(q.p)==SQLITE_ROW){
  auto ident=col(q.p,1); sqlite3_int64 g=sqlite3_column_int64(q.p,2),a=sqlite3_column_int64(q.p,3),b=sqlite3_column_int64(q.p,4),c=sqlite3_column_int64(q.p,5);
  bool identifier=(ident.size()==10||ident.size()==9)&&digits(ident);
  bool hasProfile=sqlite3_column_type(q.p,6)!=SQLITE_NULL;
  bool classification=hasProfile&&!col(q.p,6).empty()&&sqlite3_column_int(q.p,7)>0&&sqlite3_column_int(q.p,7)<=31&&!col(q.p,8).empty()&&!col(q.p,9).empty()&&!col(q.p,10).empty();
  bool fundResult=sqlite3_column_int(q.p,11)==6&&sqlite3_column_int64(q.p,12)==a&&sqlite3_column_int64(q.p,13)==b;
  bool valid=identifier&&classification&&fundResult&&g>=0&&a>=0&&b>=0&&c>=0;
  if(!valid)++issues;
  o+=std::wstring(valid?L"OK ":L"!! ")+col(q.p,0)+L" / идентификатор "+(identifier?L"въведен":L"ЛИПСВА/НЕВАЛИДЕН")+L" / брутно "+fmt(g)+L" / лични "+fmt(a)+L" / работодател "+fmt(b)+L" / данък "+fmt(c)+L"\r\n";
  if(!classification)o+=L"   !! Липсват потвърден вид осигурен, дни, НКПД, КИД или проверяващ.\r\n";
  if(!fundResult)o+=L"   !! Липсва потвърден разчет по всичките 6 фонда или сумите не съвпадат.\r\n";
  gross+=g;employee+=a;employer+=b;tax+=c;++count;
 }
 o+=L"\r\nСлужители "+std::to_wstring(count)+L", непълни записи "+std::to_wstring(issues)+L"\r\nОбщо брутно "+fmt(gross)+L" | лични "+fmt(employee)+L" | работодател "+fmt(employer)+L" | данък "+fmt(tax)+L"\r\n";
 if(!count||issues)o+=L"БЛОКИРАНО ЗА ПОДАВАНЕ: непълни данни за Д1/Д6.\r\n";
 o+=L"\r\nДори при 0 грешки този преглед не проверява всички кодове, основания, корекции, болнични, отсъствия, ограничения и реквизити по Наредба Н-13. Не създава официални Д1/Д6 файлове.\r\n";
 return o;
}
std::wstring employmentXml(State* s){int id=(int)SendMessageW(s->combo,CB_GETCURSEL,0,0);if(id<0||id>=(int)s->ids.size())return L"Изберете служител.";
 std::wstring o=L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<!-- ВЪТРЕШЕН ПРОЕКТ AME: НЕ Е НАП ЕТЗ XML -->\r\n<ame-employment-drafts xmlns=\"urn:ame:internal:employment:1\">\r\n";
 Query q;if(!prep(s->db,q,"SELECT s.name,s.identifier,s.position,e.kind,e.docno,e.docdate,e.effective,e.basis,e.terms,e.salary FROM am_employment_work e JOIN am_staff s ON s.id=e.staff WHERE s.id=? ORDER BY e.docdate,e.id")||!numParam(q,1,s->ids[id]))return L"Грешка при генериране на проекта.";
 int count=0;while(sqlite3_step(q.p)==SQLITE_ROW){o+=L"  <record>\r\n";const wchar_t* keys[]={L"name",L"identifier",L"position",L"document-number",L"document-date",L"effective-date",L"legal-basis",L"terms"};const int columns[]={0,1,2,4,5,6,7,8};for(int k=0;k<8;++k){std::wstring key=keys[k];o+=L"    <"+key+L">"+escapeXml(col(q.p,columns[k]))+L"</"+key+L">\r\n";}o+=L"    <kind>"+std::to_wstring(sqlite3_column_int(q.p,3))+L"</kind>\r\n    <salary>"+fmt(sqlite3_column_int64(q.p,9))+L"</salary>\r\n  </record>\r\n";++count;}
 o+=L"</ame-employment-drafts>\r\n";if(!count)o=L"Няма трудови документи за този служител.";return o;
}

const int maxDepRate[]={0,400,3000,1000,5000,2500,3333,1500};
std::wstring taxPreview(State* s){int sel=(int)SendMessageW(s->combo,CB_GETCURSEL,0,0);if(sel<0||sel>=(int)s->ids.size())return L"Изберете актив.";auto year=selectedYear(s);if(year.size()!=4||!digits(year))return L"Въведете година.";
 Query q;if(!prep(s->db,q,"SELECT a.name,t.category,t.basis,t.rate_bp,t.service_date FROM am_tax_assets t JOIN am_assets a ON a.id=t.asset WHERE t.asset=?")||!numParam(q,1,s->ids[sel])||sqlite3_step(q.p)!=SQLITE_ROW)return L"Първо запишете данъчния профил на актива.";
 int category=sqlite3_column_int(q.p,1),bp=sqlite3_column_int(q.p,3);auto basis=sqlite3_column_int64(q.p,2);auto start=col(q.p,4);auto o=L"ДАНЪЧЕН АМОРТИЗАЦИОНЕН ПЛАН — ПРОЕКТ\r\nАктив: "+col(q.p,0)+L"\r\nКатегория "+std::to_wstring(category)+L", база "+fmt(basis)+L", норма "+fmt(bp)+L" %, въвеждане в експлоатация "+start+L"\r\n\r\n";
 Query prior;sqlite3_int64 used=0;if(prep(s->db,prior,"SELECT COALESCE(SUM(tax),0) FROM am_depr_work WHERE asset=? AND period<?")&&numParam(prior,1,s->ids[sel])&&textParam(prior,2,year+L"-01")&&sqlite3_step(prior.p)==SQLITE_ROW)used=sqlite3_column_int64(prior.p,0);
 if(used>basis)return L"ГРЕШКА: натрупаната данъчна амортизация надвишава данъчната стойност.";
 // The annual amount is based on the months active in the tax year, capped
 // by the remaining tax basis. Rounding is distributed over active months.
 std::vector<int> eligible;for(int m=1;m<=12;++m){wchar_t buf[8];swprintf_s(buf,8,L"%02d",m);std::wstring p=year+L"-"+buf;if(p>=start.substr(0,7))eligible.push_back(m);}
 sqlite3_int64 annual=std::min(basis-used,(basis*bp*(sqlite3_int64)eligible.size()+60000)/120000);
 o+=L"Предходно начислено: "+fmt(used)+L". Планирано за "+year+L": "+fmt(annual)+L"\r\n";
 for(size_t j=0;j<eligible.size();++j){wchar_t b[8];swprintf_s(b,8,L"%02d",eligible[j]);sqlite3_int64 amount=annual/(sqlite3_int64)eligible.size()+(j<(size_t)(annual%(sqlite3_int64)eligible.size())?1:0);
  o+=year+L"-"+b+L"  "+fmt(amount)+L"\r\n";}
 o+=L"\r\nТова е изчисление по въведените от счетоводител данни. Не отчита автоматично подобрения, отписвания, специфични изключения и променени ставки.\r\n";return o;
}

bool saveTax(State* s,std::wstring& error){int sel=(int)SendMessageW(s->combo,CB_GETCURSEL,0,0),category=(int)SendMessageW(s->list,CB_GETCURSEL,0,0)+1;sqlite3_int64 basis;int rate;
 if(sel<0||sel>=(int)s->ids.size()||category<1||category>7||!money(val(s,2),basis)||basis<=0||!basisPoints(val(s,3),rate)||rate==0||rate>maxDepRate[category]||!day(val(s,4))||val(s,5).empty()){
  error=L"Изберете актив/категория, въведете база, норма под лимита, дата и отговорно лице.";return false;}
 if(isLocked(s->db,_wtoi(val(s,4).substr(0,4).c_str()))){error=L"Годината на датата е заключена.";return false;}
 Query q;return prep(s->db,q,"INSERT OR REPLACE INTO am_tax_assets(asset,category,basis,rate_bp,service_date,reviewer) VALUES(?,?,?,?,?,?)")&&numParam(q,1,s->ids[sel])&&numParam(q,2,category)&&numParam(q,3,basis)&&numParam(q,4,rate)&&textParam(q,5,val(s,4))&&textParam(q,6,val(s,5))&&sqlite3_step(q.p)==SQLITE_DONE;
}

bool generateTax(State* s,std::wstring& error){int sel=(int)SendMessageW(s->combo,CB_GETCURSEL,0,0);if(sel<0||sel>=(int)s->ids.size())return false;std::wstring year=val(s,0);if(year.size()!=4||!digits(year))return false;int y=_wtoi(year.c_str());if(isLocked(s->db,y)){error=L"Финансовата година е заключена.";return false;}
 Query a;if(!prep(s->db,a,"SELECT basis,rate_bp,service_date FROM am_tax_assets WHERE asset=?")||!numParam(a,1,s->ids[sel])||sqlite3_step(a.p)!=SQLITE_ROW)return false;
 auto basis=sqlite3_column_int64(a.p,0);int bp=sqlite3_column_int(a.p,1);auto start=col(a.p,2);std::vector<std::wstring> eligible;for(int m=1;m<=12;++m){wchar_t b[8];swprintf_s(b,8,L"%02d",m);auto p=year+L"-"+b;if(p>=start.substr(0,7))eligible.push_back(p);}
 if(eligible.empty()){error=L"Активът не е въведен в експлоатация през този период.";return false;}
 // Preserve all existing accounting depreciation. Tax values already entered
 // manually are NEVER overwritten. Zero-tax months may be filled.
 Query present;if(!prep(s->db,present,"SELECT COUNT(*) FROM am_depr_work WHERE asset=? AND period>=? AND period<=? AND tax<>0")||!numParam(present,1,s->ids[sel])||!textParam(present,2,year+L"-01")||!textParam(present,3,year+L"-12")||sqlite3_step(present.p)!=SQLITE_ROW)return false;
 if(sqlite3_column_int(present.p,0)>0){error=L"Има вече данъчни амортизации. За да избегнем презаписване, начислението е отказано.";return false;}
 Query prior;sqlite3_int64 used=0;if(!prep(s->db,prior,"SELECT COALESCE(SUM(tax),0) FROM am_depr_work WHERE asset=? AND period<?")||!numParam(prior,1,s->ids[sel])||!textParam(prior,2,year+L"-01")||sqlite3_step(prior.p)!=SQLITE_ROW)return false;used=sqlite3_column_int64(prior.p,0);if(used>basis){error=L"Данъчната база е надвишена.";return false;}
 sqlite3_int64 annual=std::min(basis-used,(basis*bp*(sqlite3_int64)eligible.size()+60000)/120000);if(annual<=0){error=L"Няма сума за начисляване.";return false;}
 if(!exec(s->db,"BEGIN IMMEDIATE"))return false;bool ok=true;
 for(size_t i=0;i<eligible.size();++i){auto amt=annual/(sqlite3_int64)eligible.size()+(i<(size_t)(annual%(sqlite3_int64)eligible.size())?1:0);
   Query update;ok=prep(s->db,update,"UPDATE am_depr_work SET tax=? WHERE asset=? AND period=? AND tax=0")&&numParam(update,1,amt)&&numParam(update,2,s->ids[sel])&&textParam(update,3,eligible[i])&&sqlite3_step(update.p)==SQLITE_DONE;
   if(ok&&sqlite3_changes(s->db.p)==0){Query insert;ok=prep(s->db,insert,"INSERT INTO am_depr_work(asset,period,accounting,tax,note) VALUES(?,?,0,?,?)")&&numParam(insert,1,s->ids[sel])&&textParam(insert,2,eligible[i])&&numParam(insert,3,amt)&&textParam(insert,4,L"Автоматична данъчна амортизация; подлежи на преглед")&&sqlite3_step(insert.p)==SQLITE_DONE;}
   if(!ok)break;}
 if(ok){Query q;ok=prep(s->db,q,"INSERT INTO am_compliance_audit(action,detail) VALUES('tax_depreciation',?)")&&textParam(q,1,L"Актив "+std::to_wstring(s->ids[sel])+L", година "+year+L", сума "+fmt(annual))&&sqlite3_step(q.p)==SQLITE_DONE;}
 if(ok&&exec(s->db,"COMMIT"))return true;exec(s->db,"ROLLBACK");error=L"Неуспешна транзакция.";return false;
}

std::wstring yearPreview(State* s){auto year=selectedYear(s);if(year.size()!=4||!digits(year))return L"Въведете година ГГГГ.";int y=_wtoi(year.c_str());std::wstring o=L"ГОДИШНО ПРИКЛЮЧВАНЕ / ПРОВЕРКА "+year+L"\r\n";
 o+=isLocked(s->db,y)?L"СТАТУС: ЗАКЛЮЧЕНА\r\n":L"СТАТУС: ОТВОРЕНА\r\n";
 Query q;if(prep(s->db,q,"SELECT COUNT(*) FROM (SELECT e.id FROM journal_entries e LEFT JOIN journal_lines l ON l.entry_id=e.id WHERE e.date BETWEEN ? AND ? GROUP BY e.id HAVING COALESCE(SUM(l.debit-l.credit),0)<>0 OR COUNT(l.entry_id)<2)")&&textParam(q,1,year+L"-01-01")&&textParam(q,2,year+L"-12-31")&&sqlite3_step(q.p)==SQLITE_ROW){int unbalanced=sqlite3_column_int(q.p,0);o+=L"Небалансирани/непълни операции: "+std::to_wstring(unbalanced)+L"\r\n";}
 Query sums;if(prep(s->db,sums,"SELECT a.code,a.name,a.kind,COALESCE(SUM(CASE WHEN e.date BETWEEN ? AND ? THEN l.debit-l.credit ELSE 0 END),0) FROM accounts a LEFT JOIN journal_lines l ON l.account_id=a.id LEFT JOIN journal_entries e ON e.id=l.entry_id WHERE a.kind IN (3,4) GROUP BY a.id ORDER BY a.code")&&textParam(sums,1,year+L"-01-01")&&textParam(sums,2,year+L"-12-31")){o+=L"\r\nОперации за приключване на номиналните сметки:\r\n";int count=0;sqlite3_int64 total=0;while(sqlite3_step(sums.p)==SQLITE_ROW){auto net=sqlite3_column_int64(sums.p,3);if(!net)continue;++count;total+=net;o+=col(sums.p,0)+L" "+col(sums.p,1)+L"  "+fmt(net)+L" (Дт-Кт)\r\n";}o+=L"Сметки с движение: "+std::to_wstring(count)+L"; нето към 123: "+fmt(total)+L"\r\n";}
 o+=L"\r\nЩе се създаде реална, балансирана счетоводна операция към 123 и годината ще се заключи с SQLite тригери. Това НЕ замества счетоводните корекции и одит преди годишното приключване.\r\n";return o;
}

bool closeYear(State* s,std::wstring& error){auto year=val(s,0),reviewer=val(s,1);if(year.size()!=4||!digits(year)||_wtoi(year.c_str())<2000||_wtoi(year.c_str())>2200||reviewer.empty()){error=L"Въведете година и име на проверяващия.";return false;}int y=_wtoi(year.c_str());if(isLocked(s->db,y)){error=L"Годината вече е приключена.";return false;}
 if(!exec(s->db,"BEGIN IMMEDIATE")){error=L"Базата е заета.";return false;}
 bool ok=true;sqlite3_int64 resultId=0;std::vector<std::pair<int,sqlite3_int64>> accounts;
 Query check;ok=prep(s->db,check,"SELECT COUNT(*) FROM (SELECT e.id FROM journal_entries e LEFT JOIN journal_lines l ON l.entry_id=e.id WHERE e.date BETWEEN ? AND ? GROUP BY e.id HAVING COALESCE(SUM(l.debit-l.credit),0)<>0 OR COUNT(l.entry_id)<2)")&&textParam(check,1,year+L"-01-01")&&textParam(check,2,year+L"-12-31")&&sqlite3_step(check.p)==SQLITE_ROW&&sqlite3_column_int(check.p,0)==0;
 if(!ok)error=L"Има небалансирани или непълни счетоводни записи.";
 Query target;int profitId=0;if(ok&&prep(s->db,target,"SELECT id FROM accounts WHERE code='123' AND active=1")&&sqlite3_step(target.p)==SQLITE_ROW)profitId=sqlite3_column_int(target.p,0);if(!profitId){ok=false;error=L"Липсва активна сметка 123 - Печалби и загуби от текущата година.";}
 Query rows;if(ok){
  ok=prep(s->db,rows,"SELECT a.id,SUM(l.debit-l.credit) FROM journal_lines l JOIN journal_entries e ON e.id=l.entry_id JOIN accounts a ON a.id=l.account_id WHERE e.date BETWEEN ? AND ? AND a.kind IN (3,4) GROUP BY a.id HAVING SUM(l.debit-l.credit)<>0")&&textParam(rows,1,year+L"-01-01")&&textParam(rows,2,year+L"-12-31");
  if(ok){int rc;while((rc=sqlite3_step(rows.p))==SQLITE_ROW)accounts.push_back({sqlite3_column_int(rows.p,0),sqlite3_column_int64(rows.p,1)});if(rc!=SQLITE_DONE)ok=false;}
 }
 if(ok&&accounts.empty()){error=L"Няма приходни/разходни движения за приключване.";ok=false;}
 if(ok){Query insert;ok=prep(s->db,insert,"INSERT INTO journal_entries(date,document,description) VALUES(?,?,?)")&&textParam(insert,1,year+L"-12-31")&&textParam(insert,2,L"AME-CLOSE-"+year)&&textParam(insert,3,L"Годишно приключване към сметка 123 - проверете корекциите")&&sqlite3_step(insert.p)==SQLITE_DONE;if(ok)resultId=sqlite3_last_insert_rowid(s->db.p);}
 sqlite3_int64 debit=0,credit=0;int seq=0;
 auto line=[&](int account,sqlite3_int64 db,sqlite3_int64 cr)->bool{Query q;return prep(s->db,q,"INSERT INTO journal_lines(entry_id,line_no,account_id,debit,credit) VALUES(?,?,?,?,?)")&&numParam(q,1,resultId)&&numParam(q,2,++seq)&&numParam(q,3,account)&&numParam(q,4,db)&&numParam(q,5,cr)&&sqlite3_step(q.p)==SQLITE_DONE;};
 if(ok){for(auto& a:accounts){auto amount=a.second;sqlite3_int64 d=amount<0?-amount:0,c=amount>0?amount:0;debit+=d;credit+=c;if(!line(a.first,d,c)){ok=false;break;}}}
 if(ok){if(debit>credit)ok=line(profitId,0,debit-credit);else if(credit>debit)ok=line(profitId,credit-debit,0);}
 if(ok){Query q;ok=prep(s->db,q,"INSERT INTO am_fiscal_year(year,locked,entry,closed_at,reviewer,note) VALUES(?,1,?,datetime('now'),?,?)")&&numParam(q,1,y)&&numParam(q,2,resultId)&&textParam(q,3,reviewer)&&textParam(q,4,L"Годишна операция към 123; резултатът подлежи на одит")&&sqlite3_step(q.p)==SQLITE_DONE;}
 if(ok){Query q;ok=prep(s->db,q,"INSERT INTO am_compliance_audit(action,detail) VALUES('close_year',?)")&&textParam(q,1,L"Година "+year+L", проверил "+reviewer+L", документ "+std::to_wstring(resultId))&&sqlite3_step(q.p)==SQLITE_DONE;}
 if(ok&&exec(s->db,"COMMIT"))return true;exec(s->db,"ROLLBACK");if(error.empty())error=L"Операцията беше отменена; няма частично приключване.";return false;
}

void fillCombo(State* s,bool assets){s->ids.clear();SendMessageW(s->combo,CB_RESETCONTENT,0,0);Query q;const char* sql=assets?"SELECT id,code||' - '||name FROM am_assets ORDER BY code":"SELECT id,name||' - '||position FROM am_staff ORDER BY name";
 if(prep(s->db,q,sql)){while(sqlite3_step(q.p)==SQLITE_ROW){auto name=col(q.p,1);SendMessageW(s->combo,CB_ADDSTRING,0,(LPARAM)name.c_str());s->ids.push_back(sqlite3_column_int(q.p,0));}}if(!s->ids.empty())SendMessageW(s->combo,CB_SETCURSEL,0,0);
}
void create(HWND h,State* s){s->hwnd=h;s->font=CreateFontW(-17,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,L"Tahoma");
 const wchar_t* title[]={L"ДДС: преглед на регистрите",L"Заплати: профил и автоматичен разчет",L"Д1/Д6: вътрешна контролна ведомост",L"ЕТЗ: работен XML (не НАП)",L"Данъчна амортизация: план",L"Годишно приключване и заключване"};
 label(h,s,title[s->mode],18,12,1050);
 if(s->mode==0||s->mode==2){field(h,s,0,L"Месец ГГГГ-ММ",20,55,240,curMonth().c_str());}
 if(s->mode==1){field(h,s,0,L"Месец ГГГГ-ММ",20,55,215,curMonth().c_str());field(h,s,1,L"Лични осигуровки %",255,55,180,L"0.00");field(h,s,2,L"Работодател %",455,55,180,L"0.00");field(h,s,3,L"Данък %",655,55,165,L"0.00");field(h,s,4,L"Мин. осиг. доход",20,113,215,L"0.00");field(h,s,5,L"Макс. осиг. доход",255,113,180,L"0.00");field(h,s,6,L"Проверил",455,113,350);}
 if(s->mode==3){label(h,s,L"Служител",20,60);s->combo=control(h,s->font,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,20,88,650,450,250);fillCombo(s,false);}
 if(s->mode==4){field(h,s,0,L"Година",20,55,180,curMonth().substr(0,4).c_str());label(h,s,L"Актив",220,55);s->combo=control(h,s->font,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,220,81,360,400,250);fillCombo(s,true);
 label(h,s,L"Категория по ЗКПО",595,55);s->list=control(h,s->font,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,595,81,320,220,260);for(int i=1;i<=7;++i){std::wstring a=L"Категория "+std::to_wstring(i)+L" — до "+fmt(maxDepRate[i])+L" %";SendMessageW(s->list,CB_ADDSTRING,0,(LPARAM)a.c_str());}SendMessageW(s->list,CB_SETCURSEL,0,0);
 field(h,s,2,L"Данъчна база",20,123,180,L"0.00");field(h,s,3,L"Данъчна норма %",220,123,180,L"4.00");field(h,s,4,L"В експлоатация от",420,123,180,now().c_str());field(h,s,5,L"Проверил",620,123,295);}
 if(s->mode==5){field(h,s,0,L"Година за приключване",20,55,220,curMonth().substr(0,4).c_str());field(h,s,1,L"Проверил (задължително)",265,55,350);}
 int y=(s->mode==1||s->mode==4)?201:146;
 btn(h,s,L"Преглед / изчисли",301,20,y,205);
 if(s->mode==1){btn(h,s,L"Запази ставки",302,240,y,180);btn(h,s,L"Приложи към ведомост",303,435,y,235);}
 if(s->mode==4){btn(h,s,L"Запази данъчен профил",302,240,y,225);btn(h,s,L"Начисли за годината",303,485,y,200);}
 if(s->mode==5)btn(h,s,L"ПРИКЛЮЧИ И ЗАКЛЮЧИ",303,245,y,290);
 btn(h,s,L"Експорт за вътрешен преглед",304,700,y,280);
 s->out=control(h,s->font,L"EDIT",L"",WS_BORDER|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL,20,y+53,1030,410,350);
 label(h,s,L"ПРЕДУПРЕЖДЕНИЕ: този екран не създава официален файл, готов за подаване в НАП.",20,y+475,1010);
}
void refresh(State* s){switch(s->mode){case 0:setOut(s,vatReview(s));break;case 1:setOut(s,payrollPreview(s));break;case 2:setOut(s,d1d6Preview(s));break;case 3:setOut(s,employmentXml(s));break;case 4:setOut(s,taxPreview(s));break;case 5:setOut(s,yearPreview(s));break;}}
LRESULT CALLBACK wndproc(HWND h,UINT msg,WPARAM w,LPARAM l){State* s=(State*)GetWindowLongPtrW(h,GWLP_USERDATA);
 if(msg==WM_NCCREATE){s=(State*)((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)s);}if(!s)return DefWindowProcW(h,msg,w,l);
 if(msg==WM_CREATE){create(h,s);refresh(s);return 0;}
 if(msg==WM_COMMAND){int id=LOWORD(w);if(id==301){refresh(s);return 0;}
  if(id==302){std::wstring error;bool ok=s->mode==1?saveRates(s,error):s->mode==4?saveTax(s,error):false;say(h,ok?L"Записано в базата на избраната фирма.":error.empty()?L"Записът не е успешен.":error,ok);refresh(s);return 0;}
  if(id==303){if(s->mode==5){if(MessageBoxW(h,L"ВНИМАНИЕ! Реална счетоводна операция и заключване на всички периоди от годината.\nПърво архивирайте фирмената база и проверете всички приключвателни операции.\nПродължавате ли?",L"AME",MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING)!=IDYES)return 0;}
   else if(MessageBoxW(h,L"Това ще промени запазени счетоводни данни. Проверени ли са данните от счетоводител?",L"AME",MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING)!=IDYES)return 0;
   std::wstring error;bool ok=s->mode==1?applyRates(s,error):s->mode==4?generateTax(s,error):s->mode==5?closeYear(s,error):false;say(h,ok?L"Успешна операция. Данните са записани.":error.empty()?L"Операцията е неуспешна.":error,ok);refresh(s);return 0;}
  if(id==304){refresh(s);if(s->preview.empty())return 0;if(MessageBoxW(h,L"Това е САМО вътрешен преглед, НЕ НАП файл. Да се запази ли?",L"AME",MB_YESNO|MB_ICONWARNING)!=IDYES)return 0;bool ok=saveUtf8(h,s->preview,L"AME — вътрешен преглед");if(!ok)say(h,L"Не беше запазен файл.");return 0;}
 }
 if(msg==WM_CLOSE){DestroyWindow(h);return 0;}
 if(msg==WM_DESTROY){if(s->font)DeleteObject(s->font);return 0;}
 return DefWindowProcW(h,msg,w,l);
}
} // namespace

void ShowAMECompliance(HWND parent,int companyId,const std::wstring& name,int mode){if(mode<0||mode>5||companyId<=0)return;State s;s.company=companyId;s.mode=mode;s.companyName=name;
 if(!openCompany(s.db,companyId)){say(parent,L"Грешка при отваряне на счетоводната база.");return;}
 WNDCLASSW wc={};wc.lpfnWndProc=wndproc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.lpszClassName=L"AMECompliance050";RegisterClassW(&wc);
 std::wstring title=L"AME — "+name+L" — професионални проверки";
 HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,title.c_str(),WS_CAPTION|WS_SYSMENU|WS_POPUP,CW_USEDEFAULT,CW_USEDEFAULT,1080,735,parent,nullptr,wc.hInstance,&s);if(!h)return;
 EnableWindow(parent,FALSE);ShowWindow(h,SW_SHOW);MSG m;while(IsWindow(h)){int n=GetMessageW(&m,nullptr,0,0);if(n<=0){if(n==0)PostQuitMessage((int)m.wParam);break;}if(!IsDialogMessageW(h,&m)){TranslateMessage(&m);DispatchMessageW(&m);}}
 if(IsWindow(h))DestroyWindow(h);EnableWindow(parent,TRUE);SetActiveWindow(parent);
}
