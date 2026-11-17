#include "professional_modules.h"
#include "../data/ame_calculations.h"
#include "fund_calculator.h"
#include "../data/chart_template.h"
#include <windows.h>
#include <shlobj.h>
#include <commdlg.h>
#include <sqlite3.h>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cwchar>
#include <limits>

namespace {
struct Db { sqlite3* h=nullptr; ~Db(){if(h)sqlite3_close(h);} };
struct Stmt { sqlite3_stmt* h=nullptr; ~Stmt(){if(h)sqlite3_finalize(h);} };
bool sql(Db& d,const char* q){return sqlite3_exec(d.h,q,nullptr,nullptr,nullptr)==SQLITE_OK;}
bool prepare(Db& d,Stmt& s,const char* q){return sqlite3_prepare_v2(d.h,q,-1,&s.h,nullptr)==SQLITE_OK;}
bool bind(Stmt& s,int i,const std::wstring& v){return sqlite3_bind_text16(s.h,i,v.c_str(),-1,SQLITE_TRANSIENT)==SQLITE_OK;}
bool bind64(Stmt& s,int i,sqlite3_int64 v){return sqlite3_bind_int64(s.h,i,v)==SQLITE_OK;}
std::wstring txt(sqlite3_stmt* s,int i){const void* v=sqlite3_column_text16(s,i);return v?(const wchar_t*)v:L"";}
std::wstring get(HWND h){if(!h)return L"";int n=GetWindowTextLengthW(h);std::vector<wchar_t> b(n+1);GetWindowTextW(h,b.data(),n+1);return b.data();}
std::wstring trim(const std::wstring& s){size_t a=s.find_first_not_of(L" \r\n\t"),b=s.find_last_not_of(L" \r\n\t");return a==std::wstring::npos?L"":s.substr(a,b-a+1);}
bool num(const std::wstring& s){return !s.empty()&&s.find_first_not_of(L"0123456789")==std::wstring::npos;}
bool date(const std::wstring& s){if(s.size()!=10||s[4]!=L'-'||s[7]!=L'-')return false;std::wstring n=s.substr(0,4)+s.substr(5,2)+s.substr(8,2);if(!num(n))return false;int y=_wtoi(s.substr(0,4).c_str()),m=_wtoi(s.substr(5,2).c_str()),d=_wtoi(s.substr(8,2).c_str());int ds[]={31,28,31,30,31,30,31,31,30,31,30,31};return y>=1900&&y<=2200&&m>=1&&m<=12&&d>=1&&d<=ds[m-1]+(m==2&&y%4==0&&(y%100!=0||y%400==0));}
bool period(const std::wstring& s){return s.size()==7&&s[4]==L'-'&&num(s.substr(0,4))&&num(s.substr(5,2))&&_wtoi(s.substr(0,4).c_str())>=1900&&_wtoi(s.substr(0,4).c_str())<=2200&&_wtoi(s.substr(5,2).c_str())>=1&&_wtoi(s.substr(5,2).c_str())<=12;}
std::wstring money(sqlite3_int64 v){bool neg=v<0;if(neg)v=-v;return(neg?L"-":L"")+std::to_wstring(v/100)+L"."+(v%100<10?L"0":L"")+std::to_wstring(v%100);}
bool cents(const std::wstring& raw,sqlite3_int64& v,bool allowNegative=false){std::wstring s=trim(raw);v=0;if(s.empty())return false;bool neg=s[0]==L'-';if(neg){if(!allowNegative)return false;s=s.substr(1);}size_t pos=s.find_first_of(L".,");std::wstring a=pos==std::wstring::npos?s:s.substr(0,pos),b=pos==std::wstring::npos?L"":s.substr(pos+1);if(!num(a)||b.size()>2||(!b.empty()&&!num(b))||a.size()>12)return false;v=_wtoi64(a.c_str())*100;if(b.size()==1)v+=10*(b[0]-L'0');if(b.size()==2)v+=10*(b[0]-L'0')+b[1]-L'0';if(neg)v=-v;return v>=-90000000000000LL&&v<=90000000000000LL;}
std::wstring today(){SYSTEMTIME t;GetLocalTime(&t);wchar_t b[20];swprintf_s(b,20,L"%04u-%02u-%02u",t.wYear,t.wMonth,t.wDay);return b;}
std::wstring currentPeriod(){return today().substr(0,7);}
void alert(HWND h,const std::wstring& t,bool ok){MessageBoxW(h,t.c_str(),L"AME",MB_OK|(ok?MB_ICONINFORMATION:MB_ICONWARNING));}
bool openDb(Db& d,int id){if(id<=0)return false;wchar_t app[MAX_PATH]={};if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE,nullptr,SHGFP_TYPE_CURRENT,app)))return false;std::wstring folder=std::wstring(app)+L"\\AME";CreateDirectoryW(folder.c_str(),nullptr);folder+=L"\\companies";CreateDirectoryW(folder.c_str(),nullptr);std::wstring p=folder+L"\\company_"+std::to_wstring(id)+L".db";if(sqlite3_open16(p.c_str(),&d.h)!=SQLITE_OK)return false;sqlite3_busy_timeout(d.h,5000);return EnsureChartTemplate(d.h)&&sql(d,"PRAGMA foreign_keys=ON;")&&sql(d,
 "CREATE TABLE IF NOT EXISTS am_staff(id INTEGER PRIMARY KEY,name TEXT NOT NULL,identifier TEXT NOT NULL,position TEXT NOT NULL,start_date TEXT NOT NULL,note TEXT NOT NULL);"
 "CREATE TABLE IF NOT EXISTS am_assets(id INTEGER PRIMARY KEY,code TEXT NOT NULL UNIQUE,name TEXT NOT NULL,acquired TEXT NOT NULL,price INTEGER NOT NULL,life_months INTEGER NOT NULL,note TEXT NOT NULL);"
 "CREATE TABLE IF NOT EXISTS am_vat_work(id INTEGER PRIMARY KEY,period TEXT NOT NULL,direction INTEGER NOT NULL,kind TEXT NOT NULL,number TEXT NOT NULL,date TEXT NOT NULL,partner TEXT NOT NULL,vat_id TEXT NOT NULL,base INTEGER NOT NULL,vat INTEGER NOT NULL,treatment TEXT NOT NULL,currency TEXT NOT NULL,note TEXT NOT NULL,created TEXT NOT NULL,UNIQUE(period,direction,number,vat_id));"
 "CREATE INDEX IF NOT EXISTS am_vat_period ON am_vat_work(period,direction);"
 "CREATE TABLE IF NOT EXISTS am_pay_work(id INTEGER PRIMARY KEY,period TEXT NOT NULL,staff INTEGER NOT NULL REFERENCES am_staff(id),gross INTEGER NOT NULL,employee_ins INTEGER NOT NULL,employer_ins INTEGER NOT NULL,tax INTEGER NOT NULL,other INTEGER NOT NULL,note TEXT NOT NULL,UNIQUE(period,staff));"
 "CREATE TABLE IF NOT EXISTS am_employment_work(id INTEGER PRIMARY KEY,staff INTEGER NOT NULL REFERENCES am_staff(id),kind INTEGER NOT NULL,docno TEXT NOT NULL,docdate TEXT NOT NULL,effective TEXT NOT NULL,basis TEXT NOT NULL,terms TEXT NOT NULL,salary INTEGER NOT NULL,note TEXT NOT NULL,UNIQUE(kind,docno));"
 "CREATE TABLE IF NOT EXISTS am_depr_work(id INTEGER PRIMARY KEY,asset INTEGER NOT NULL REFERENCES am_assets(id),period TEXT NOT NULL,accounting INTEGER NOT NULL,tax INTEGER NOT NULL,note TEXT NOT NULL,UNIQUE(asset,period));"
 "CREATE TABLE IF NOT EXISTS am_year_review(year INTEGER PRIMARY KEY,reviewer TEXT NOT NULL,notes TEXT NOT NULL,reviewed_at TEXT NOT NULL,diagnostic TEXT NOT NULL);"
 "CREATE TABLE IF NOT EXISTS am_audit(id INTEGER PRIMARY KEY,at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,module TEXT NOT NULL,action TEXT NOT NULL,record_id INTEGER NOT NULL);"
 "CREATE TRIGGER IF NOT EXISTS am_vat_ins AFTER INSERT ON am_vat_work BEGIN INSERT INTO am_audit(module,action,record_id) VALUES('vat','insert',NEW.id); END;"
 "CREATE TRIGGER IF NOT EXISTS am_vat_upd AFTER UPDATE ON am_vat_work BEGIN INSERT INTO am_audit(module,action,record_id) VALUES('vat','update',NEW.id); END;"
 "CREATE TRIGGER IF NOT EXISTS am_pay_ins AFTER INSERT ON am_pay_work BEGIN INSERT INTO am_audit(module,action,record_id) VALUES('payroll','insert',NEW.id); END;"
 "CREATE TRIGGER IF NOT EXISTS am_pay_upd AFTER UPDATE ON am_pay_work BEGIN INSERT INTO am_audit(module,action,record_id) VALUES('payroll','update',NEW.id); END;"
 "CREATE TRIGGER IF NOT EXISTS am_emp_ins AFTER INSERT ON am_employment_work BEGIN INSERT INTO am_audit(module,action,record_id) VALUES('employment','insert',NEW.id); END;"
 "CREATE TRIGGER IF NOT EXISTS am_emp_upd AFTER UPDATE ON am_employment_work BEGIN INSERT INTO am_audit(module,action,record_id) VALUES('employment','update',NEW.id); END;"
 "CREATE TRIGGER IF NOT EXISTS am_depr_ins AFTER INSERT ON am_depr_work BEGIN INSERT INTO am_audit(module,action,record_id) VALUES('depreciation','insert',NEW.id); END;"
 "CREATE TRIGGER IF NOT EXISTS am_depr_upd AFTER UPDATE ON am_depr_work BEGIN INSERT INTO am_audit(module,action,record_id) VALUES('depreciation','update',NEW.id); END;"
 "CREATE TRIGGER IF NOT EXISTS am_year_ins AFTER INSERT ON am_year_review BEGIN INSERT INTO am_audit(module,action,record_id) VALUES('year-review','insert',NEW.year); END;"
 "CREATE TRIGGER IF NOT EXISTS am_year_upd AFTER UPDATE ON am_year_review BEGIN INSERT INTO am_audit(module,action,record_id) VALUES('year-review','update',NEW.year); END;"
 );}

struct Field {const wchar_t* label;bool combo;};
// Field counts are fixed so a single, consistent editor is used throughout.
static const Field vatFields[]={
 {L"Данъчен период (ГГГГ-ММ)",false},{L"Дневник",true},{L"Вид документ",true},
 {L"Номер на документ",false},{L"Дата (ГГГГ-ММ-ДД)",false},{L"Контрагент",false},
 {L"ДДС номер на контрагент",false},{L"Данъчна основа",false},{L"ДДС (може автоматично)",false},
 {L"Данъчно третиране (вътрешен код)",false},{L"Валута",true},{L"Бележка",false}
};
static const Field payFields[]={
 {L"Месец (ГГГГ-ММ)",false},{L"Служител",true},{L"Брутно възнаграждение",false},
 {L"Лични осигуровки",false},{L"Осигуровки работодател",false},
 {L"Данък общ доход",false},{L"Други удръжки (въведени)",false},{L"Бележка",false}
};
static const Field empFields[]={
 {L"Служител",true},{L"Вид документ",true},{L"Номер",false},{L"Дата",false},
 {L"В сила от",false},{L"Правно основание (проверете)",false},{L"Условия / предмет",false},
 {L"Основна заплата",false},{L"Бележка",false}
};
static const Field depFields[]={
 {L"Актив",true},{L"Месец",false},{L"Счетоводна амортизация",false},
 {L"Данъчна амортизация (ръчно)",false},{L"Бележка",false}
};
static const Field yearFields[]={
 {L"Година",false},{L"Проверил",false},{L"Бележка",false}
};
struct State {
 Db db; HFONT font=nullptr; HWND list=nullptr,summary=nullptr,output=nullptr,controls[12]={};
 HWND parent=nullptr; int mode=0, suggested=0, selected=0,company=0;
 int count=0;std::wstring name;std::vector<int> ids,employeeIds,assetIds;
 std::wstring preview;
};
HWND ctrl(HWND parent,HFONT font,const wchar_t* cls,const wchar_t* content,DWORD flags,int x,int y,int w,int h,int id=0){HWND c=CreateWindowExW(0,cls,content,WS_CHILD|WS_VISIBLE|flags,x,y,w,h,parent,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);if(c)SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);return c;}
void button(HWND h,State* s,const wchar_t* t,int x,int y,int w,int id){ctrl(h,s->font,L"BUTTON",t,WS_TABSTOP|BS_PUSHBUTTON,x,y,w,32,id);}
void set(HWND h,const std::wstring& text){if(h)SetWindowTextW(h,text.c_str());}
std::wstring val(State* s,int i){return trim(get(s->controls[i]));}
int chosen(State* s,int i){return (int)SendMessageW(s->controls[i],CB_GETCURSEL,0,0);}
int selectId(State* s,int i){int n=chosen(s,i);const auto& ids=s->mode==3?s->assetIds:s->employeeIds;return n>=0&&n<(int)ids.size()?ids[n]:0;}
void selectById(State* s,int i,int id){const auto& ids=s->mode==3?s->assetIds:s->employeeIds;for(size_t a=0;a<ids.size();++a)if(ids[a]==id){SendMessageW(s->controls[i],CB_SETCURSEL,(WPARAM)a,0);return;}}
void populatePeople(State* s,HWND combo,bool assets){const char* q=assets?"SELECT id,code||' — '||name FROM am_assets ORDER BY code":"SELECT id,name||' — '||position FROM am_staff ORDER BY name";Stmt st;if(!prepare(s->db,st,q))return;auto& ids=assets?s->assetIds:s->employeeIds;ids.clear();while(sqlite3_step(st.h)==SQLITE_ROW){ids.push_back(sqlite3_column_int(st.h,0));std::wstring name=txt(st.h,1);SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)name.c_str());}if(!ids.empty())SendMessageW(combo,CB_SETCURSEL,0,0);}
void fresh(State* s){s->selected=0;SendMessageW(s->list,LB_SETCURSEL,(WPARAM)-1,0);for(int i=0;i<s->count;++i){if(s->mode==0&&(i==1||i==2||i==10))SendMessageW(s->controls[i],CB_SETCURSEL,i==1&&s->suggested==1?1:0,0);else if((s->mode==1&&i==1)||(s->mode==2&&(i==0||i==1))||(s->mode==3&&i==0))SendMessageW(s->controls[i],CB_SETCURSEL,0,0);else set(s->controls[i],L"");}
 if(s->mode==0){set(s->controls[0],currentPeriod());set(s->controls[4],today());set(s->controls[7],L"0.00");set(s->controls[8],L"0.00");set(s->controls[9],L"STANDARD");}
 if(s->mode==1){set(s->controls[0],currentPeriod());for(int i=2;i<=6;++i)set(s->controls[i],L"0.00");}
 if(s->mode==2){set(s->controls[3],today());set(s->controls[4],today());set(s->controls[7],L"0.00");}
 if(s->mode==3){set(s->controls[1],currentPeriod());set(s->controls[2],L"0.00");set(s->controls[3],L"0.00");}
 if(s->mode==4)set(s->controls[0],today().substr(0,4));
 if(s->mode==2&&s->suggested>=0&&s->suggested<=6)SendMessageW(s->controls[1],CB_SETCURSEL,s->suggested,0);
}
void list(State* s){s->ids.clear();SendMessageW(s->list,LB_RESETCONTENT,0,0);const char* sqls[]={
 "SELECT id,period,direction,number,partner,base,vat FROM am_vat_work ORDER BY period DESC,date DESC,id DESC",
 "SELECT p.id,p.period,s.name,p.gross,p.employee_ins,p.tax,p.other FROM am_pay_work p JOIN am_staff s ON s.id=p.staff ORDER BY p.period DESC,s.name",
 "SELECT e.id,e.docdate,e.docno,s.name,e.kind FROM am_employment_work e JOIN am_staff s ON s.id=e.staff ORDER BY e.docdate DESC,e.id DESC",
 "SELECT d.id,d.period,a.code,a.name,d.accounting,d.tax FROM am_depr_work d JOIN am_assets a ON a.id=d.asset ORDER BY d.period DESC,a.code",
 "SELECT year,reviewer,reviewed_at FROM am_year_review ORDER BY year DESC"
 };Stmt q;if(!prepare(s->db,q,sqls[s->mode]))return;while(sqlite3_step(q.h)==SQLITE_ROW){int id=sqlite3_column_int(q.h,0);s->ids.push_back(id);std::wstring line;
 if(s->mode==0)line=txt(q.h,1)+L" | "+(sqlite3_column_int(q.h,2)?L"Продажба":L"Покупка")+L" | "+txt(q.h,3)+L" | "+txt(q.h,4)+L" | "+money(sqlite3_column_int64(q.h,5))+L" + "+money(sqlite3_column_int64(q.h,6));
 if(s->mode==1)line=txt(q.h,1)+L" | "+txt(q.h,2)+L" | Бруто "+money(sqlite3_column_int64(q.h,3));
 if(s->mode==2)line=txt(q.h,1)+L" | №"+txt(q.h,2)+L" | "+txt(q.h,3);
 if(s->mode==3)line=txt(q.h,1)+L" | "+txt(q.h,2)+L" | "+txt(q.h,3)+L" | "+money(sqlite3_column_int64(q.h,4));
 if(s->mode==4)line=std::to_wstring(id)+L" | "+txt(q.h,1)+L" | "+txt(q.h,2);
 SendMessageW(s->list,LB_ADDSTRING,0,(LPARAM)line.c_str());}
 set(s->summary,L"Записи: "+std::to_wstring(s->ids.size())+L"   |   Данни само за избраната фирма");}
void load(State* s){int ix=(int)SendMessageW(s->list,LB_GETCURSEL,0,0);if(ix<0||ix>=(int)s->ids.size())return;s->selected=s->ids[ix];Stmt q;const char* queries[]={
 "SELECT period,direction,kind,number,date,partner,vat_id,base,vat,treatment,currency,note FROM am_vat_work WHERE id=?",
 "SELECT period,staff,gross,employee_ins,employer_ins,tax,other,note FROM am_pay_work WHERE id=?",
 "SELECT staff,kind,docno,docdate,effective,basis,terms,salary,note FROM am_employment_work WHERE id=?",
 "SELECT asset,period,accounting,tax,note FROM am_depr_work WHERE id=?",
 "SELECT year,reviewer,notes FROM am_year_review WHERE year=?"
 };if(!prepare(s->db,q,queries[s->mode])||!bind64(q,1,s->selected)||sqlite3_step(q.h)!=SQLITE_ROW)return;
 for(int i=0;i<s->count;++i){
  if(s->mode==0&&(i==1||i==2||i==10)){
   int n=i==1?sqlite3_column_int(q.h,i):i==2?0:0;
   if(i==2){std::wstring code=txt(q.h,i);const wchar_t* cs[]={L"01",L"02",L"03",L"09"};for(int j=0;j<4;++j)if(code==cs[j])n=j;}
   if(i==10)n=txt(q.h,i)==L"BGN"?1:0;
   SendMessageW(s->controls[i],CB_SETCURSEL,n,0);
  }else if((s->mode==1&&i==1)||(s->mode==2&&i==0)||(s->mode==3&&i==0))selectById(s,i,sqlite3_column_int(q.h,i));
  else if(s->mode==2&&i==1)SendMessageW(s->controls[i],CB_SETCURSEL,sqlite3_column_int(q.h,i),0);
  else if((s->mode==0&&(i==7||i==8))||(s->mode==1&&(i>=2&&i<=6))||(s->mode==2&&i==7)||(s->mode==3&&(i==2||i==3)))set(s->controls[i],money(sqlite3_column_int64(q.h,i)));
  else if(s->mode==4&&i==0)set(s->controls[i],std::to_wstring(sqlite3_column_int(q.h,0)));
  else set(s->controls[i],txt(q.h,i));
 }
}
const wchar_t* kVat[]={L"01",L"02",L"03",L"09"};
bool save(State* s,std::wstring& error){
 Stmt q;bool update=s->selected!=0;int x=s->selected;
 if(s->mode==0){
  auto p=val(s,0),number=val(s,3),day=val(s,4),partner=val(s,5),vat=val(s,6),treat=val(s,9),note=val(s,11);sqlite3_int64 base=0,tax=0;int dir=chosen(s,1),kind=chosen(s,2),cur=chosen(s,10);
  if(!period(p)||!date(day)||number.empty()||partner.empty()||treat.empty()||dir<0||dir>1||kind<0||kind>3||cur<0||cur>1||!cents(val(s,7),base,true)||!cents(val(s,8),tax,true)||(!base&&!tax)||(base>0&&tax<0)||(base<0&&tax>0)){
   error=L"Проверете периода, датата, номера, контрагента и сумите. ДДС и основата трябва да са със съвместими знаци.";return false;
  }
  if(treat==L"AME_ZERO_MISSING_BASIS"){
   error=L"За 0% данък въведете конкретното правно основание в „Данъчно третиране“; празен шаблон не се запазва.";return false;
  }
  if(treat==L"AME_AUTO_20"||treat==L"AME_AUTO_9_VERIFY"){
   ame::Money expected=0,full=0;
   if(!ame::VatFromBase(base,treat==L"AME_AUTO_20"?2000:900,expected,full)||expected!=tax){
    error=L"Сумата на ДДС не съответства на автоматично избраната ставка. Преизчислете ДДС или сменете данъчното третиране.";return false;
   }
  }
  // An already-posted invoice must not be silently entered again into the
  // manual VAT register, which would distort monthly totals.
  Stmt existing;
  if(prepare(s->db,existing,"SELECT COUNT(*) FROM invoices i JOIN partners p ON p.id=i.partner WHERE i.direction=? AND i.number=? AND i.date>=? AND i.date<? AND ((?<>'' AND p.vat=?) OR (?='' AND p.name=?))")&&bind64(existing,1,dir)&&bind(existing,2,number)&&bind(existing,3,p+L"-01")&&bind(existing,4,p+L"-32")&&bind(existing,5,vat)&&bind(existing,6,vat)&&bind(existing,7,vat)&&bind(existing,8,partner)&&sqlite3_step(existing.h)==SQLITE_ROW&&sqlite3_column_int(existing.h,0)>0){
   error=L"Същата фактура вече е записана в стария модул и се включва в прегледа. Не я дублирайте.";return false;
  }
  if(!prepare(s->db,q,update?
   "UPDATE am_vat_work SET period=?,direction=?,kind=?,number=?,date=?,partner=?,vat_id=?,base=?,vat=?,treatment=?,currency=?,note=? WHERE id=?":
   "INSERT INTO am_vat_work(period,direction,kind,number,date,partner,vat_id,base,vat,treatment,currency,note,created) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)"))return false;
  bool ok=bind(q,1,p)&&bind64(q,2,dir)&&bind(q,3,kVat[kind])&&bind(q,4,number)&&bind(q,5,day)&&bind(q,6,partner)&&bind(q,7,vat)&&bind64(q,8,base)&&bind64(q,9,tax)&&bind(q,10,treat)&&bind(q,11,cur==0?L"EUR":L"BGN")&&bind(q,12,note)&&(update?bind64(q,13,x):bind(q,13,today()));if(!ok)return false;
 } else if(s->mode==1){
  int id=selectId(s,1);auto p=val(s,0);sqlite3_int64 amounts[5]={};bool ok=true;for(int i=0;i<5;++i)ok=ok&&cents(val(s,i+2),amounts[i]);
  if(!period(p)||!id||!ok||amounts[0]<amounts[1]+amounts[3]+amounts[4]){error=L"Изберете служител и период. Осигуровки, данък и удръжки не могат да надвишават брутното възнаграждение.";return false;}
  if(!prepare(s->db,q,update?
   "UPDATE am_pay_work SET period=?,staff=?,gross=?,employee_ins=?,employer_ins=?,tax=?,other=?,note=? WHERE id=?":
   "INSERT INTO am_pay_work(period,staff,gross,employee_ins,employer_ins,tax,other,note) VALUES(?,?,?,?,?,?,?,?)"))return false;
  ok=bind(q,1,p)&&bind64(q,2,id);for(int i=0;i<5;++i)ok=ok&&bind64(q,i+3,amounts[i]);ok=ok&&bind(q,8,val(s,7));if(update)ok=ok&&bind64(q,9,x);if(!ok)return false;
 }else if(s->mode==2){
  int id=selectId(s,0),kind=chosen(s,1);auto number=val(s,2),day=val(s,3),effective=val(s,4);sqlite3_int64 salary;
  if(!id||kind<0||kind>6||number.empty()||!date(day)||!date(effective)||val(s,5).empty()||!cents(val(s,7),salary)){
   error=L"Изберете служител, тип документ, номер, валидни дати, правно основание и заплата.";return false;
  }
  if(!prepare(s->db,q,update?
   "UPDATE am_employment_work SET staff=?,kind=?,docno=?,docdate=?,effective=?,basis=?,terms=?,salary=?,note=? WHERE id=?":
   "INSERT INTO am_employment_work(staff,kind,docno,docdate,effective,basis,terms,salary,note) VALUES(?,?,?,?,?,?,?,?,?)"))return false;
  bool ok=bind64(q,1,id)&&bind64(q,2,kind)&&bind(q,3,number)&&bind(q,4,day)&&bind(q,5,effective)&&bind(q,6,val(s,5))&&bind(q,7,val(s,6))&&bind64(q,8,salary)&&bind(q,9,val(s,8));if(update)ok=ok&&bind64(q,10,x);if(!ok)return false;
 }else if(s->mode==3){
  int id=selectId(s,0);auto p=val(s,1);sqlite3_int64 account,tax;
  if(!id||!period(p)||!cents(val(s,2),account)||!cents(val(s,3),tax)){error=L"Изберете актив, месец и неотрицателни суми.";return false;}
  // Never silently depreciate more than the acquisition cost.
  Stmt check;if(!prepare(s->db,check,"SELECT price,acquired FROM am_assets WHERE id=?")||!bind64(check,1,id)||sqlite3_step(check.h)!=SQLITE_ROW)return false;
  sqlite3_int64 price=sqlite3_column_int64(check.h,0);auto acquired=txt(check.h,1);
  if(p+L"-01"<acquired.substr(0,7)+L"-01"){error=L"Периодът е преди датата на придобиване.";return false;}
  Stmt sum;if(!prepare(s->db,sum,"SELECT COALESCE(SUM(accounting),0) FROM am_depr_work WHERE asset=? AND id<>?")||!bind64(sum,1,id)||!bind64(sum,2,x)||sqlite3_step(sum.h)!=SQLITE_ROW)return false;
  if(sqlite3_column_int64(sum.h,0)+account>price){error=L"Счетоводната амортизация надвишава отчетната стойност на актива.";return false;}
  if(!prepare(s->db,q,update?
   "UPDATE am_depr_work SET asset=?,period=?,accounting=?,tax=?,note=? WHERE id=?":
   "INSERT INTO am_depr_work(asset,period,accounting,tax,note) VALUES(?,?,?,?,?)"))return false;
  bool ok=bind64(q,1,id)&&bind(q,2,p)&&bind64(q,3,account)&&bind64(q,4,tax)&&bind(q,5,val(s,4));if(update)ok=ok&&bind64(q,6,x);if(!ok)return false;
 }else{
  auto year=val(s,0),reviewer=val(s,1);if(year.size()!=4||!num(year)||_wtoi(year.c_str())<1900||_wtoi(year.c_str())>2200||reviewer.empty()){
   error=L"Въведете година и име на проверяващия.";return false;
  }
  if(!prepare(s->db,q,"INSERT OR REPLACE INTO am_year_review(year,reviewer,notes,reviewed_at,diagnostic) VALUES(?,?,?,?,?)")||!bind64(q,1,_wtoi(year.c_str()))||!bind(q,2,reviewer)||!bind(q,3,val(s,2))||!bind(q,4,today())||!bind(q,5,s->preview))return false;
 }
 if(sqlite3_step(q.h)!=SQLITE_DONE){error=L"Неуспешен запис. Възможен дублиран документ/месец или грешка в данните.";return false;}return true;
}
std::wstring yearCheck(State* s){auto year=val(s,0);if(year.size()!=4||!num(year))return L"Въведете година, например 2026.";
 std::wstring start=year+L"-01-01",end=year+L"-12-31";
 std::wstring o=L"ГОДИШНА ПРЕДПРИКЛЮЧВАТЕЛНА ПРОВЕРКА — "+year+L"\r\n\r\n";
 const char* q="SELECT COUNT(DISTINCT e.id),COALESCE(SUM(l.debit),0),COALESCE(SUM(l.credit),0) FROM journal_entries e LEFT JOIN journal_lines l ON l.entry_id=e.id WHERE e.date>=? AND e.date<=?";
 Stmt z;if(prepare(s->db,z,q)&&bind(z,1,start)&&bind(z,2,end)&&sqlite3_step(z.h)==SQLITE_ROW){auto d=sqlite3_column_int64(z.h,1),c=sqlite3_column_int64(z.h,2);o+=L"Счетоводни операции: "+std::to_wstring(sqlite3_column_int(z.h,0))+L"\r\nДебит: "+money(d)+L" | Кредит: "+money(c)+(d==c?L" (равни)":L" (РАЗЛИКА!)")+L"\r\n";}else o+=L"Счетоводният журнал не е наличен.\r\n";
 const char* checks[]={"SELECT COUNT(*) FROM am_vat_work WHERE period>=? AND period<=?", "SELECT COUNT(*) FROM am_pay_work WHERE period>=? AND period<=?", "SELECT COUNT(*) FROM am_employment_work WHERE docdate>=? AND docdate<=?", "SELECT COUNT(*) FROM am_depr_work WHERE period>=? AND period<=?"};
 const wchar_t* captions[]={L"Вътрешни ДДС записи",L"Месечни записи за заплати",L"Документи на персонала",L"Амортизационни записи"};
 for(int i=0;i<4;++i){Stmt r;if(prepare(s->db,r,checks[i])&&bind(r,1,i==0||i==1||i==3?year+L"-01":start)&&bind(r,2,i==0||i==1||i==3?year+L"-12":end)&&sqlite3_step(r.h)==SQLITE_ROW)o+=std::wstring(captions[i])+L": "+std::to_wstring(sqlite3_column_int(r.h,0))+L"\r\n";}
 o+=L"\r\nТози преглед НЕ приключва сметките, НЕ блокира минали периоди и НЕ замества годишните счетоводни процедури.\r\n";
 return o;
}
std::wstring preview(State* s){
 std::wstring o;
 if(s->mode==0){
  std::wstring p=val(s,0);if(!period(p))p=currentPeriod();sqlite3_int64 bases[2][2]={{0,0},{0,0}},taxes[2][2]={{0,0},{0,0}};int rows=0,legacyRows=0;
  o=L"ДДС РАБОТЕН РЕГИСТЪР — "+p+L"\r\n";
  o+=L"Вид | Номер | Дата | Контрагент | Основа | ДДС | Вал.\r\n";
  Stmt q;if(prepare(s->db,q,"SELECT direction,number,date,partner,base,vat,currency FROM am_vat_work WHERE period=? ORDER BY direction,date,number")&&bind(q,1,p)){
   while(sqlite3_step(q.h)==SQLITE_ROW){int dir=sqlite3_column_int(q.h,0);auto base=sqlite3_column_int64(q.h,4),vat=sqlite3_column_int64(q.h,5);int cur=txt(q.h,6)==L"BGN"?1:0;bases[cur][dir]+=base;taxes[cur][dir]+=vat;++rows;
    o+=(dir?L"Продажба":L"Покупка")+std::wstring(L" | ")+txt(q.h,1)+L" | "+txt(q.h,2)+L" | "+txt(q.h,3)+L" | "+money(base)+L" | "+money(vat)+L" | "+txt(q.h,6)+L"\r\n";}
  }
  // Legacy invoices were already posted by the older AME module. Include
  // them in a distinct section; they lack statutory tax-treatment metadata.
  Stmt legacy;if(prepare(s->db,legacy,"SELECT i.direction,i.number,i.date,p.name,i.net,i.vat FROM invoices i JOIN partners p ON p.id=i.partner WHERE i.date>=? AND i.date<? ORDER BY i.direction,i.date,i.id")&&bind(legacy,1,p+L"-01")&&bind(legacy,2,p+L"-32")){
   o+=L"\r\nФактури от стария модул (непълна ДДС класификация):\r\n";
   while(sqlite3_step(legacy.h)==SQLITE_ROW){int dir=sqlite3_column_int(legacy.h,0);auto base=sqlite3_column_int64(legacy.h,4),vat=sqlite3_column_int64(legacy.h,5);++legacyRows;
    o+=(dir?L"Продажба":L"Покупка")+std::wstring(L" | ")+txt(legacy.h,1)+L" | "+txt(legacy.h,2)+L" | "+txt(legacy.h,3)+L" | "+money(base)+L" | "+money(vat)+L"\r\n";}
  }
  o+=L"\r\nВътрешни документи: "+std::to_wstring(rows)+L" | Стари фактури без валутни данни: "+std::to_wstring(legacyRows)+L"\r\n";
  for(int cur=0;cur<2;++cur){
   o+=(cur==0?L"EUR":L"BGN")+std::wstring(L": покупки основа ")+money(bases[cur][0])+L", ДДС "+money(taxes[cur][0])+L"; продажби основа "+money(bases[cur][1])+L", ДДС "+money(taxes[cur][1])+L"\r\n";
  }
  o+=L"Стари фактури са показани отделно и НЕ са включени в сумите по валута.\r\n";
  o+=L"ВНИМАНИЕ: данъчните режими не са разпределени по официалните колони на ЗДДС. НЕ е справка-декларация и НЕ е файл за НАП.\r\n";
 } else if(s->mode==1){
  std::wstring p=val(s,0);if(!period(p))p=currentPeriod();o=L"РАЗЧЕТНА ВЕДОМОСТ (ВЪТРЕШНА) — "+p+L"\r\n";
  o+=L"Служител | Брутно | Лични осиг. | Данък | Друго | Нетно | Осиг. работодател\r\n";
  Stmt q;if(prepare(s->db,q,"SELECT s.name,p.gross,p.employee_ins,p.tax,p.other,p.employer_ins FROM am_pay_work p JOIN am_staff s ON s.id=p.staff WHERE p.period=? ORDER BY s.name")&&bind(q,1,p)){
   sqlite3_int64 total=0,gross=0,employer=0;
   while(sqlite3_step(q.h)==SQLITE_ROW){auto g=sqlite3_column_int64(q.h,1),si=sqlite3_column_int64(q.h,2),tax=sqlite3_column_int64(q.h,3),other=sqlite3_column_int64(q.h,4),er=sqlite3_column_int64(q.h,5);auto net=g-si-tax-other;total+=net;gross+=g;employer+=er;
    o+=txt(q.h,0)+L" | "+money(g)+L" | "+money(si)+L" | "+money(tax)+L" | "+money(other)+L" | "+money(net)+L" | "+money(er)+L"\r\n";}
   o+=L"\r\nОбщо бруто: "+money(gross)+L" | Нетно: "+money(total)+L" | Разход работодател: "+money(gross+employer)+L"\r\n";
  }
  o+=L"ВНИМАНИЕ: от меню Заплати -> Настройка на заплатите можете да изчислите суми по фондове от проверени ставки. Няма универсална законова автоматизация за всяка категория. НЕ е Д1/Д6.\r\n";
 }else if(s->mode==2){
  const wchar_t* kinds[]={L"Трудов договор",L"Допълнително споразумение",L"Прекратяване",L"Прекласиране",L"Заповед за отпуск",L"Присъствена форма",L"ЕТЗ - работен запис"};
  int kind=chosen(s,1);if(kind<0||kind>6)kind=0;
  o=L"ПРОЕКТ — "+std::wstring(kinds[kind])+L"\r\n";
  o+=L"Документ № "+val(s,2)+L" / "+val(s,3)+L"\r\n";
  int id=selectId(s,0);Stmt q;
  if(prepare(s->db,q,"SELECT name,position,start_date FROM am_staff WHERE id=?")&&bind64(q,1,id)&&sqlite3_step(q.h)==SQLITE_ROW)o+=L"Служител: "+txt(q.h,0)+L" | Длъжност: "+txt(q.h,1)+L"\r\n";
  o+=L"В сила от: "+val(s,4)+L"\r\nПравно основание: "+val(s,5)+L"\r\nУсловия: "+val(s,6)+L"\r\nОсновна заплата: "+val(s,7)+L"\r\nБележка: "+val(s,8)+L"\r\n";
  o+=L"\r\nНЕПОДПИСАН ПРОЕКТ. Нужни са реквизити, проверка от специалист по ТРЗ и одобрен шаблон. НЕ е подаден ЕТЗ или официален XML.\r\n";
 }else if(s->mode==3){
  o=L"АМОРТИЗАЦИОННА ВЕДОМОСТ — ВЪТРЕШЕН РАБОТЕН ДОКУМЕНТ\r\n";
  o+=L"Месец | Актив | Счетоводна | Данъчна (въведена)\r\n";
  Stmt q;sqlite3_int64 total=0,tax=0;
  if(prepare(s->db,q,"SELECT d.period,a.code||' '||a.name,d.accounting,d.tax FROM am_depr_work d JOIN am_assets a ON a.id=d.asset ORDER BY d.period,a.code")){
   while(sqlite3_step(q.h)==SQLITE_ROW){auto a=sqlite3_column_int64(q.h,2),t=sqlite3_column_int64(q.h,3);total+=a;tax+=t;o+=txt(q.h,0)+L" | "+txt(q.h,1)+L" | "+money(a)+L" | "+money(t)+L"\r\n";}
  }
  o+=L"\r\nОбщо счетоводни: "+money(total)+L" | Данъчни: "+money(tax)+L"\r\n";
  o+=L"НЕ са създадени счетоводни операции. Данъчните амортизации се въвеждат ръчно и НЕ са автоматичен данъчен план по ЗКПО.\r\n";
 }else o=yearCheck(s);
 return o;
}
void refresh(HWND h,State* s){(void)h;s->preview=preview(s);set(s->output,s->preview);}
void automaticVAT(HWND h,State* s){
 ame::Money base=0,vat=0,gross=0;
 if(!ame::ParseMoney(val(s,7),base,true)||base==0){
  alert(h,L"Първо въведете в полето за основата сума (напр. 100.00). Ако сумата включва ДДС, изберете специалната опция от менюто.",false);return;
 }
 HMENU menu=CreatePopupMenu();
 AppendMenuW(menu,MF_STRING,4101,L"20% — стандартна ставка");
 AppendMenuW(menu,MF_STRING,4102,L"9% — само за приложими доставки по чл. 66а");
 AppendMenuW(menu,MF_STRING,4103,L"0% — само с посочено правно основание");
 AppendMenuW(menu,MF_STRING,4104,L"20% — изчисли ДДС от обща сума с данък");
 AppendMenuW(menu,MF_STRING,4105,L"9% — изчисли ДДС от обща сума с данък");
 POINT pt={};GetCursorPos(&pt);
 UINT choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,pt.x,pt.y,0,h,nullptr);
 DestroyMenu(menu);
 if(choice<4101||choice>4105)return;
 const bool grossInput=choice==4104||choice==4105;
 int bp=(choice==4101||choice==4104)?2000:(choice==4102||choice==4105)?900:0;
 if(bp==900&&MessageBoxW(h,L"9% важи само за доставки, които отговарят на условията по ЗДДС. Потвърждавате ли, че счетоводител е избрал правилната ставка?",L"AME — проверка на 9%",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES)return;
 if(bp==0&&MessageBoxW(h,L"При 0% или неначисляване на ДДС е задължително правилното правно основание. Ще трябва да го въведете ръчно в „Данъчно третиране“. Продължавате ли?",L"AME — правно основание",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES)return;
 if(grossInput){
  gross=base;
  if(!ame::VatFromGross(gross,bp,base,vat)){alert(h,L"Неуспешно изчисление от обща сума.",false);return;}
  set(s->controls[7],ame::FormatMoney(base));
 }else if(!ame::VatFromBase(base,bp,vat,gross)){alert(h,L"Неуспешно изчисление. Проверете основата.",false);return;}
 set(s->controls[8],ame::FormatMoney(vat));
 set(s->controls[9],bp==2000?L"AME_AUTO_20":bp==900?L"AME_AUTO_9_VERIFY":L"AME_ZERO_MISSING_BASIS");
 std::wstring text=L"Основа: "+ame::FormatMoney(base)+L" | ДДС: "+ame::FormatMoney(vat)+L" | Общо: "+ame::FormatMoney(gross)+L".\n";
 text+=bp==0?L"Въведете правното основание на мястото на AME_ZERO_MISSING_BASIS преди запис.":L"Проверете приложимостта на ставката преди запис.";
 alert(h,text,true);
}
void suggestDep(State* s){int id=selectId(s,0);Stmt q;if(!id||!prepare(s->db,q,"SELECT acquired,price,life_months FROM am_assets WHERE id=?")||!bind64(q,1,id)||sqlite3_step(q.h)!=SQLITE_ROW)return;
 auto acquired=txt(q.h,0);sqlite3_int64 price=sqlite3_column_int64(q.h,1);int life=sqlite3_column_int(q.h,2);if(life<1)return;
 auto p=val(s,1);if(!period(p)||acquired.size()<7)return;
 int startY=_wtoi(acquired.substr(0,4).c_str()),startM=_wtoi(acquired.substr(5,2).c_str());
 // Only a suggested straight-line amount: the accountant must verify the
 // depreciation commencement date and applicable accounting policy.
 int yy=_wtoi(p.substr(0,4).c_str()),mm=_wtoi(p.substr(5,2).c_str());int months=(yy-startY)*12+mm-startM;
 if(months<0||months>=life){alert(s->parent,L"Месецът е извън предложения срок на амортизация, считано от месеца на придобиване.",false);return;}
 sqlite3_int64 monthly=price/life;if(months==life-1)monthly+=price%life;set(s->controls[2],money(monthly));
 alert(s->parent,L"Предложена линейна счетоводна амортизация. Начална дата и амортизационна политика изискват проверка. Данъчната сума не е изчислена.",true);
}
bool exportPreview(State* s){std::wstring t=s->preview;if(t.empty())return false;
 OPENFILENAMEW ofn={};wchar_t filename[MAX_PATH]={};
 ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=s->parent;ofn.lpstrFilter=L"Text workpaper (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0";
 ofn.lpstrFile=filename;ofn.nMaxFile=MAX_PATH;ofn.lpstrDefExt=L"txt";ofn.Flags=OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;
 if(!GetSaveFileNameW(&ofn))return false;
 int bytes=WideCharToMultiByte(CP_UTF8,0,t.c_str(),(int)t.size(),nullptr,0,nullptr,nullptr);if(bytes<=0)return false;
 std::vector<char> out(bytes);if(WideCharToMultiByte(CP_UTF8,0,t.c_str(),(int)t.size(),out.data(),bytes,nullptr,nullptr)!=bytes)return false;
 HANDLE f=CreateFileW(filename,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return false;
 DWORD n=0;const unsigned char bom[3]={0xEF,0xBB,0xBF};bool ok=WriteFile(f,bom,3,&n,nullptr)&&n==3;
 if(ok)ok=WriteFile(f,out.data(),(DWORD)out.size(),&n,nullptr)&&n==(DWORD)out.size();
 CloseHandle(f);if(!ok)DeleteFileW(filename);return ok;
}
LRESULT CALLBACK wndproc(HWND h,UINT msg,WPARAM w,LPARAM l){
 State* s=(State*)GetWindowLongPtrW(h,GWLP_USERDATA);
 if(msg==WM_NCCREATE){s=(State*)((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)s);}
 if(!s)return DefWindowProcW(h,msg,w,l);
 if(msg==WM_CREATE){
  s->parent=h;s->font=CreateFontW(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Tahoma");
  const wchar_t* headings[]={L"ДДС — вътрешни регистри (НЕ НАП файл)",L"Заплати — месечни разчети (проверка на ставки)",L"Назначения — проекти на документи",L"Амортизации — счетоводен и данъчен разчет",L"Годишна проверка — без реално приключване"};
  ctrl(h,s->font,L"STATIC",headings[s->mode],0,15,12,1070,28);
  ctrl(h,s->font,L"STATIC",L"Запазени записи — изберете за редакция",0,15,47,435,24);
  s->list=ctrl(h,s->font,L"LISTBOX",L"",LBS_NOTIFY|WS_VSCROLL|WS_BORDER|WS_TABSTOP,15,77,435,426,100);
  s->summary=ctrl(h,s->font,L"STATIC",L"",0,15,509,435,24);
  const Field* fields=s->mode==0?vatFields:s->mode==1?payFields:s->mode==2?empFields:s->mode==3?depFields:yearFields;
  s->count=s->mode==0?12:s->mode==1?8:s->mode==2?9:s->mode==3?5:3;
  for(int i=0;i<s->count;++i){int x=468+(i%2)*314,y=48+(i/2)*80;
   ctrl(h,s->font,L"STATIC",fields[i].label,0,x,y,310,23);
   DWORD flags=fields[i].combo?CBS_DROPDOWNLIST|WS_VSCROLL:ES_AUTOHSCROLL|WS_BORDER;
   s->controls[i]=ctrl(h,s->font,fields[i].combo?L"COMBOBOX":L"EDIT",L"",flags|WS_TABSTOP,x,y+26,301,fields[i].combo?250:27,200+i);
  }
  if(s->mode==0){const wchar_t* dir[]={L"Покупка",L"Продажба"};for(auto t:dir)SendMessageW(s->controls[1],CB_ADDSTRING,0,(LPARAM)t);
   const wchar_t* kinds[]={L"01 Фактура",L"02 Дебитно известие",L"03 Кредитно известие",L"09 Протокол"};for(auto t:kinds)SendMessageW(s->controls[2],CB_ADDSTRING,0,(LPARAM)t);
   SendMessageW(s->controls[10],CB_ADDSTRING,0,(LPARAM)L"EUR");SendMessageW(s->controls[10],CB_ADDSTRING,0,(LPARAM)L"BGN");}
  if(s->mode==1)populatePeople(s,s->controls[1],false);
  if(s->mode==2){populatePeople(s,s->controls[0],false);const wchar_t* kinds[]={L"Трудов договор",L"Допълнително споразумение",L"Прекратяване",L"Прекласиране",L"Заповед за отпуск",L"Присъствена форма",L"ЕТЗ (вътрешен запис)"};for(auto t:kinds)SendMessageW(s->controls[1],CB_ADDSTRING,0,(LPARAM)t);}
  if(s->mode==3)populatePeople(s,s->controls[0],true);
  button(h,s,L"Нов запис",15,546,135,301);
  button(h,s,L"Запази",160,546,135,302);
  button(h,s,L"Преглед",306,546,145,303);
  if(s->mode==0)button(h,s,L"Изчисли ДДС (20/9/0)",469,546,192,304);
  if(s->mode==1)button(h,s,L"Разчет по фондове",469,546,192,304);
  if(s->mode==3)button(h,s,L"Предложи месечна",469,546,190,304);
  if(s->mode==4)button(h,s,L"Проверка",469,546,190,304);
  button(h,s,L"Експорт на проект .txt",670,546,250,305);
  button(h,s,L"Затвори",932,546,150,306);
  s->output=ctrl(h,s->font,L"EDIT",L"",ES_MULTILINE|ES_READONLY|WS_BORDER|WS_VSCROLL|WS_HSCROLL|ES_AUTOHSCROLL|ES_AUTOVSCROLL,15,595,1065,94,350);
  fresh(s);list(s);refresh(h,s);
  if(s->mode==1&&s->employeeIds.empty())alert(h,L"Първо създайте служители от Заплати > Библиотека на персонала.",false);
  if(s->mode==3&&s->assetIds.empty())alert(h,L"Първо създайте активи в Амортизации > Инвентарна книга.",false);
  return 0;
 }
 if(msg==WM_COMMAND){int id=LOWORD(w),notify=HIWORD(w);
  if(id==100&&notify==LBN_SELCHANGE){load(s);refresh(h,s);return 0;}
  if(id==301){fresh(s);refresh(h,s);return 0;}
  if(id==302){if(s->mode==4){s->preview=yearCheck(s);set(s->output,s->preview);if(MessageBoxW(h,L"Записване на проверката? Това НЕ извършва счетоводно приключване.",L"AME",MB_YESNO|MB_ICONQUESTION)!=IDYES)return 0;}
   std::wstring err;bool ok=save(s,err);if(ok){fresh(s);list(s);refresh(h,s);alert(h,L"Записано. Данните са в базата на избраната фирма.",true);}else alert(h,err.empty()?L"Неуспешен запис. Проверете данните и дублираните записи.":err,false);return 0;}
  if(id==303){refresh(h,s);return 0;}
  if(id==304){if(s->mode==0)automaticVAT(h,s);else if(s->mode==1)ShowAMEFundCalculator(h,s->company,s->name);else if(s->mode==3)suggestDep(s);else if(s->mode==4)refresh(h,s);return 0;}
  if(id==305){refresh(h,s);if(exportPreview(s))alert(h,L"Запазен е вътрешен проектен документ. Това НЕ е файл за НАП.",true);return 0;}
  if(id==306){DestroyWindow(h);return 0;}
 }
 if(msg==WM_CLOSE){DestroyWindow(h);return 0;}
 if(msg==WM_DESTROY){if(s->font)DeleteObject(s->font);return 0;}
 return DefWindowProcW(h,msg,w,l);
}
} // namespace
void ShowAMEProfessional(HWND parent,int companyId,const std::wstring& companyName,int mode,int suggested){
 if(mode<0||mode>4||companyId<=0)return;State s;s.company=companyId;s.mode=mode;s.suggested=suggested;s.name=companyName;
 if(!openDb(s.db,companyId)){alert(parent,L"Неуспешно отваряне/обновяване на базата. Направете архив на данните и проверете правата за запис.",false);return;}
 static const wchar_t* cls=L"AMEProfessionalWorkspaces040";WNDCLASSW wc={};wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=cls;wc.lpfnWndProc=wndproc;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);RegisterClassW(&wc);
 std::wstring title=L"AME — "+companyName+L" — работна функционалност";
 HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,cls,title.c_str(),WS_CAPTION|WS_SYSMENU|WS_POPUP,CW_USEDEFAULT,CW_USEDEFAULT,1110,740,parent,nullptr,wc.hInstance,&s);
 if(!h)return;EnableWindow(parent,FALSE);ShowWindow(h,SW_SHOW);MSG msg;
 while(IsWindow(h)){BOOL n=GetMessageW(&msg,nullptr,0,0);if(n<=0){if(n==0)PostQuitMessage((int)msg.wParam);break;}if(!IsDialogMessageW(h,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
 if(IsWindow(h))DestroyWindow(h);EnableWindow(parent,TRUE);SetActiveWindow(parent);
}
