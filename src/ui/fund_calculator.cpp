#include "fund_calculator.h"
#include "../data/ame_calculations.h"
#include <windows.h>
#include <shlobj.h>
#include <sqlite3.h>
#include <string>
#include <vector>
#include <array>
#include <cwchar>

namespace {
struct Db {sqlite3* p=nullptr;~Db(){if(p)sqlite3_close(p);}};
struct Q {sqlite3_stmt* p=nullptr;~Q(){if(p)sqlite3_finalize(p);}};
bool sql(Db& d,const char* s){return sqlite3_exec(d.p,s,nullptr,nullptr,nullptr)==SQLITE_OK;}
bool prep(Db& d,Q& q,const char* s){return sqlite3_prepare_v2(d.p,s,-1,&q.p,nullptr)==SQLITE_OK;}
bool b(Q& q,int i,const std::wstring& s){return sqlite3_bind_text16(q.p,i,s.c_str(),-1,SQLITE_TRANSIENT)==SQLITE_OK;}
bool n(Q& q,int i,sqlite3_int64 value){return sqlite3_bind_int64(q.p,i,value)==SQLITE_OK;}
std::wstring txt(sqlite3_stmt* q,int i){auto p=sqlite3_column_text16(q,i);return p?static_cast<const wchar_t*>(p):L"";}
std::wstring get(HWND h){if(!h)return L"";int len=GetWindowTextLengthW(h);std::vector<wchar_t> v(len+1);GetWindowTextW(h,v.data(),len+1);return v.data();}
void set(HWND h,const std::wstring& s){if(h)SetWindowTextW(h,s.c_str());}
std::wstring current(){SYSTEMTIME t;GetLocalTime(&t);wchar_t a[16];swprintf_s(a,16,L"%04u-%02u",t.wYear,t.wMonth);return a;}
bool validPeriod(const std::wstring& p){return p.size()==7&&p[4]==L'-'&&p.substr(0,4).find_first_not_of(L"0123456789")==std::wstring::npos&&p.substr(5,2).find_first_not_of(L"0123456789")==std::wstring::npos&&_wtoi(p.substr(0,4).c_str())>=2000&&_wtoi(p.substr(5,2).c_str())>=1&&_wtoi(p.substr(5,2).c_str())<=12;}
void info(HWND h,const std::wstring& text,bool success=false){MessageBoxW(h,text.c_str(),L"AME — Разчет на осигуровки",MB_OK|(success?MB_ICONINFORMATION:MB_ICONWARNING));}
bool openCompany(Db& d,int id){
 wchar_t base[MAX_PATH]={};if(id<=0||FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE,nullptr,SHGFP_TYPE_CURRENT,base)))return false;
 std::wstring folder=std::wstring(base)+L"\\AME";CreateDirectoryW(folder.c_str(),nullptr);folder+=L"\\companies";CreateDirectoryW(folder.c_str(),nullptr);
 std::wstring path=folder+L"\\company_"+std::to_wstring(id)+L".db";
 if(sqlite3_open16(path.c_str(),&d.p)!=SQLITE_OK)return false;
 sqlite3_busy_timeout(d.p,5000);
 const char* schema=
 "PRAGMA foreign_keys=ON;"
 "CREATE TABLE IF NOT EXISTS am_staff(id INTEGER PRIMARY KEY,name TEXT NOT NULL,identifier TEXT NOT NULL,position TEXT NOT NULL,start_date TEXT NOT NULL,note TEXT NOT NULL);"
 "CREATE TABLE IF NOT EXISTS am_pay_work(id INTEGER PRIMARY KEY,period TEXT NOT NULL,staff INTEGER NOT NULL,gross INTEGER NOT NULL,employee_ins INTEGER NOT NULL,employer_ins INTEGER NOT NULL,tax INTEGER NOT NULL,other INTEGER NOT NULL,note TEXT NOT NULL,UNIQUE(period,staff));"
 "CREATE TABLE IF NOT EXISTS am_fiscal_year(year INTEGER PRIMARY KEY,locked INTEGER NOT NULL DEFAULT 0,entry INTEGER,closed_at TEXT NOT NULL DEFAULT '',reviewer TEXT NOT NULL DEFAULT '',note TEXT NOT NULL DEFAULT '');"
 "CREATE TABLE IF NOT EXISTS am_pay_fund_profile(period TEXT NOT NULL,staff INTEGER NOT NULL,insurance_base INTEGER NOT NULL,tax_relief INTEGER NOT NULL,tax_bp INTEGER NOT NULL,reviewer TEXT NOT NULL,insured_type TEXT NOT NULL,worked_days INTEGER NOT NULL,nkpd TEXT NOT NULL,kid TEXT NOT NULL,updated_at TEXT NOT NULL,PRIMARY KEY(period,staff));"
 "CREATE TABLE IF NOT EXISTS am_pay_fund_rates(period TEXT NOT NULL,staff INTEGER NOT NULL,fund INTEGER NOT NULL,employee_bp INTEGER NOT NULL,employer_bp INTEGER NOT NULL,PRIMARY KEY(period,staff,fund));"
 "CREATE TABLE IF NOT EXISTS am_pay_fund_result(period TEXT NOT NULL,staff INTEGER NOT NULL,fund INTEGER NOT NULL,employee_amount INTEGER NOT NULL,employer_amount INTEGER NOT NULL,calculated_at TEXT NOT NULL,PRIMARY KEY(period,staff,fund));"
 "CREATE TABLE IF NOT EXISTS am_pay_category_rates(from_period TEXT NOT NULL,insured_type TEXT NOT NULL,fund INTEGER NOT NULL,employee_bp INTEGER NOT NULL,employer_bp INTEGER NOT NULL,reviewer TEXT NOT NULL,updated_at TEXT NOT NULL,PRIMARY KEY(from_period,insured_type,fund));"
 "CREATE TABLE IF NOT EXISTS am_pay_fund_audit(id INTEGER PRIMARY KEY,at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,period TEXT NOT NULL,staff INTEGER NOT NULL,action TEXT NOT NULL,reviewer TEXT NOT NULL,description TEXT NOT NULL);"
 "CREATE TRIGGER IF NOT EXISTS amf_lock_prof_ins BEFORE INSERT ON am_pay_fund_profile WHEN EXISTS(SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year=CAST(substr(NEW.period,1,4) AS INTEGER)) BEGIN SELECT RAISE(ABORT,'AME: fiscal year locked'); END;"
 "CREATE TRIGGER IF NOT EXISTS amf_lock_prof_upd BEFORE UPDATE ON am_pay_fund_profile WHEN EXISTS(SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year IN (CAST(substr(OLD.period,1,4) AS INTEGER),CAST(substr(NEW.period,1,4) AS INTEGER))) BEGIN SELECT RAISE(ABORT,'AME: fiscal year locked'); END;"
 "CREATE TRIGGER IF NOT EXISTS amf_lock_rates_del BEFORE DELETE ON am_pay_fund_rates WHEN EXISTS(SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year=CAST(substr(OLD.period,1,4) AS INTEGER)) BEGIN SELECT RAISE(ABORT,'AME: fiscal year locked'); END;"
 "CREATE TRIGGER IF NOT EXISTS amf_lock_rates_ins BEFORE INSERT ON am_pay_fund_rates WHEN EXISTS(SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year=CAST(substr(NEW.period,1,4) AS INTEGER)) BEGIN SELECT RAISE(ABORT,'AME: fiscal year locked'); END;"
 "CREATE TRIGGER IF NOT EXISTS amf_lock_results_ins BEFORE INSERT ON am_pay_fund_result WHEN EXISTS(SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year=CAST(substr(NEW.period,1,4) AS INTEGER)) BEGIN SELECT RAISE(ABORT,'AME: fiscal year locked'); END;"
 "CREATE TRIGGER IF NOT EXISTS amf_lock_results_del BEFORE DELETE ON am_pay_fund_result WHEN EXISTS(SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year=CAST(substr(OLD.period,1,4) AS INTEGER)) BEGIN SELECT RAISE(ABORT,'AME: fiscal year locked'); END;"
 "CREATE TRIGGER IF NOT EXISTS amf_lock_category_ins BEFORE INSERT ON am_pay_category_rates WHEN EXISTS(SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year=CAST(substr(NEW.from_period,1,4) AS INTEGER)) BEGIN SELECT RAISE(ABORT,'AME: fiscal year locked'); END;"
 "CREATE TRIGGER IF NOT EXISTS amf_lock_category_update BEFORE UPDATE ON am_pay_category_rates WHEN EXISTS(SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year IN (CAST(substr(OLD.from_period,1,4) AS INTEGER),CAST(substr(NEW.from_period,1,4) AS INTEGER))) BEGIN SELECT RAISE(ABORT,'AME: fiscal year locked'); END;"
 "CREATE TRIGGER IF NOT EXISTS amf_lock_category_delete BEFORE DELETE ON am_pay_category_rates WHEN EXISTS(SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year=CAST(substr(OLD.from_period,1,4) AS INTEGER)) BEGIN SELECT RAISE(ABORT,'AME: fiscal year locked'); END;";
 return sql(d,schema);
}
struct State {
 Db db;int company=0;std::wstring companyName;HWND hwnd=nullptr,combo=nullptr,out=nullptr,period=nullptr,base=nullptr,relief=nullptr,tax=nullptr,reviewer=nullptr,insuredKind=nullptr,days=nullptr,nkpd=nullptr,kid=nullptr;
 std::array<HWND,ame::kFundCount> emp{},employer{};
 HFONT font=nullptr;std::vector<int> staffIds;std::wstring lastPreview;
};
HWND control(HWND parent,State* s,const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id=0){HWND w=CreateWindowExW(0,cls,text,WS_VISIBLE|WS_CHILD|style,x,y,width,height,parent,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);SendMessageW(w,WM_SETFONT,(WPARAM)s->font,TRUE);return w;}
void label(HWND h,State* s,const wchar_t* title,int x,int y,int w){control(h,s,L"STATIC",title,0,x,y,w,22);}
HWND edit(HWND h,State* s,int x,int y,int w,const wchar_t* text=L""){return control(h,s,L"EDIT",text,WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,x,y,w,26);}
void button(HWND h,State* s,const wchar_t* text,int id,int x,int y,int w){control(h,s,L"BUTTON",text,BS_PUSHBUTTON|WS_TABSTOP,x,y,w,34,id);}
int selectedStaff(State* s){int ix=(int)SendMessageW(s->combo,CB_GETCURSEL,0,0);return ix>=0&&ix<(int)s->staffIds.size()?s->staffIds[ix]:0;}
bool locked(State* s,const std::wstring& p){Q q;return prep(s->db,q,"SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year=?")&&n(q,1,_wtoi(p.substr(0,4).c_str()))&&sqlite3_step(q.p)==SQLITE_ROW;}
void updateStaffList(State* s){
 SendMessageW(s->combo,CB_RESETCONTENT,0,0);s->staffIds.clear();Q q;
 if(prep(s->db,q,"SELECT id,name||' — '||position FROM am_staff ORDER BY name"))while(sqlite3_step(q.p)==SQLITE_ROW){s->staffIds.push_back(sqlite3_column_int(q.p,0));auto t=txt(q.p,1);SendMessageW(s->combo,CB_ADDSTRING,0,(LPARAM)t.c_str());}
 if(!s->staffIds.empty())SendMessageW(s->combo,CB_SETCURSEL,0,0);
}
bool readWork(State* s,ame::Money& gross,ame::Money& other){
 int staff=selectedStaff(s);gross=other=0;auto period=get(s->period);Q q;
 if(!staff||!validPeriod(period)||!prep(s->db,q,"SELECT gross,other FROM am_pay_work WHERE period=? AND staff=?")||!b(q,1,period)||!n(q,2,staff)||sqlite3_step(q.p)!=SQLITE_ROW)return false;
 gross=sqlite3_column_int64(q.p,0);other=sqlite3_column_int64(q.p,1);return true;
}
void load(State* s){
 for(int i=0;i<ame::kFundCount;++i){set(s->emp[i],L"0.00");set(s->employer[i],L"0.00");}
 set(s->tax,L"10.00");set(s->relief,L"0.00");set(s->reviewer,L"");set(s->insuredKind,L"");set(s->days,L"");set(s->nkpd,L"");set(s->kid,L"");set(s->out,L"");
 ame::Money gross=0,other=0;bool has=readWork(s,gross,other);
 set(s->base,has?ame::FormatMoney(gross):L"0.00");
 auto p=get(s->period);int staff=selectedStaff(s);if(!staff||!validPeriod(p))return;
 Q q;if(prep(s->db,q,"SELECT insurance_base,tax_relief,tax_bp,reviewer,insured_type,worked_days,nkpd,kid FROM am_pay_fund_profile WHERE period=? AND staff=?")&&b(q,1,p)&&n(q,2,staff)&&sqlite3_step(q.p)==SQLITE_ROW){
  set(s->base,ame::FormatMoney(sqlite3_column_int64(q.p,0)));set(s->relief,ame::FormatMoney(sqlite3_column_int64(q.p,1)));
  set(s->tax,ame::FormatMoney(sqlite3_column_int(q.p,2)));set(s->reviewer,txt(q.p,3));set(s->insuredKind,txt(q.p,4));set(s->days,std::to_wstring(sqlite3_column_int(q.p,5)));set(s->nkpd,txt(q.p,6));set(s->kid,txt(q.p,7));
 }
 Q r;if(prep(s->db,r,"SELECT fund,employee_bp,employer_bp FROM am_pay_fund_rates WHERE period=? AND staff=?")&&b(r,1,p)&&n(r,2,staff))while(sqlite3_step(r.p)==SQLITE_ROW){int i=sqlite3_column_int(r.p,0);if(i>=0&&i<ame::kFundCount){set(s->emp[i],ame::FormatMoney(sqlite3_column_int(r.p,1)));set(s->employer[i],ame::FormatMoney(sqlite3_column_int(r.p,2)));}}
 if(!has)set(s->out,L"Няма ведомост за този служител/месец. Въведете брутна заплата в Заплати > Въвеждане на начисления и удръжки.");
}
bool collect(State* s,ame::PayrollInput& v,std::wstring& err){
 auto p=get(s->period);if(!validPeriod(p)||!selectedStaff(s)){err=L"Невалиден месец или липсва служител.";return false;}
 ame::Money gross=0,other=0;if(!readWork(s,gross,other)){err=L"Първо въведете брутна заплата в месечната ведомост на служителя.";return false;}
 v={};v.gross=gross;v.otherDeductions=other;
 if(!ame::ParseMoney(get(s->base),v.insuranceBase)||!ame::ParseMoney(get(s->relief),v.taxRelief)||!ame::ParsePercentBp(get(s->tax),v.incomeTaxBp)){
  err=L"Проверете осигурителната основа, облекченията и данъчния процент.";return false;
 }
 int total=0;
 for(int i=0;i<ame::kFundCount;++i){
  if(!ame::ParsePercentBp(get(s->emp[i]),v.funds[i].employeeBp)||!ame::ParsePercentBp(get(s->employer[i]),v.funds[i].employerBp)){
   err=L"Невалидна ставка във фонд "+std::to_wstring(i+1);return false;
  }
  total+=v.funds[i].employeeBp+v.funds[i].employerBp;
 }
 if(total<=0){err=L"Няма въведени осигурителни ставки по фондове. Не се използват измислени проценти.";return false;}
 if(get(s->reviewer).empty()){err=L"Въведете име на лицето, потвърдило ставките.";return false;}
 ame::PayrollResult check;
 if(!ame::Payroll(v,check,err))return false;
 return true;
}
// Same validation as collect(), without leaking a temporary result.
bool preview(State* s,ame::PayrollInput& input,ame::PayrollResult& result,std::wstring& err){
 if(!collect(s,input,err))return false;
 return ame::Payroll(input,result,err);
}
void showPreview(State* s){
 ame::PayrollInput in;ame::PayrollResult result;std::wstring err;
 if(!preview(s,in,result,err)){set(s->out,L"ОТКАЗ: "+err);return;}
 std::wstring o=L"ПРЕГЛЕД — НЕ Е ОФИЦИАЛНА ВЕДОМОСТ/Д1/Д6\r\n";
 o+=L"Бруто "+ame::FormatMoney(in.gross)+L"; Осигурителен доход "+ame::FormatMoney(in.insuranceBase)+L"; Данъчно облекчение "+ame::FormatMoney(in.taxRelief)+L"\r\n";
 for(int i=0;i<ame::kFundCount;++i)o+=std::wstring(ame::kFundNames[i])+L": лични "+ame::FormatMoney(result.employeeByFund[i])+L", работодател "+ame::FormatMoney(result.employerByFund[i])+L"\r\n";
 o+=L"Лични общо "+ame::FormatMoney(result.employeeInsurance)+L" | Работодател "+ame::FormatMoney(result.employerInsurance)+L"\r\n";
 o+=L"Данъчна основа "+ame::FormatMoney(result.taxableIncome)+L" | ДОД "+ame::FormatMoney(result.incomeTax)+L"\r\n";
 o+=L"НЕТНО "+ame::FormatMoney(result.net)+L" | Разход работодател "+ame::FormatMoney(result.employerCost)+L"\r\n";
 o+=L"Ставките и осигурителната основа трябва да са съобразени с категория, КИД, възраст, отработено време, болнични и други случаи. Няма автоматична юридическа проверка.";
 set(s->out,o);
}
bool audit(State* s,const std::wstring& action,const std::wstring& details){Q q;return prep(s->db,q,"INSERT INTO am_pay_fund_audit(period,staff,action,reviewer,description) VALUES(?,?,?,?,?)")&&b(q,1,get(s->period))&&n(q,2,selectedStaff(s))&&b(q,3,action)&&b(q,4,get(s->reviewer))&&b(q,5,details)&&sqlite3_step(q.p)==SQLITE_DONE;}
bool saveProfile(State* s,std::wstring& err){
 ame::PayrollInput input;ame::PayrollResult result;
 if(!preview(s,input,result,err))return false;
 auto p=get(s->period);int staff=selectedStaff(s);
 if(locked(s,p)){err=L"Тази финансова година е заключена.";return false;}
 if(!sql(s->db,"BEGIN IMMEDIATE")){err=L"Базата е заета.";return false;}
 bool ok=false;int days=0;auto strDays=get(s->days);
 if(!strDays.empty()){if(strDays.find_first_not_of(L"0123456789")!=std::wstring::npos){err=L"Невалидни отработени дни.";sql(s->db,"ROLLBACK");return false;}days=_wtoi(strDays.c_str());if(days>31){err=L"Отработените дни трябва да са до 31.";sql(s->db,"ROLLBACK");return false;}}
 Q q;ok=prep(s->db,q,"INSERT OR REPLACE INTO am_pay_fund_profile(period,staff,insurance_base,tax_relief,tax_bp,reviewer,insured_type,worked_days,nkpd,kid,updated_at) VALUES(?,?,?,?,?,?,?,?,?,?,datetime('now'))")&&b(q,1,p)&&n(q,2,staff)&&n(q,3,input.insuranceBase)&&n(q,4,input.taxRelief)&&n(q,5,input.incomeTaxBp)&&b(q,6,get(s->reviewer))&&b(q,7,get(s->insuredKind))&&n(q,8,days)&&b(q,9,get(s->nkpd))&&b(q,10,get(s->kid))&&sqlite3_step(q.p)==SQLITE_DONE;
 if(ok){Q del;ok=prep(s->db,del,"DELETE FROM am_pay_fund_rates WHERE period=? AND staff=?")&&b(del,1,p)&&n(del,2,staff)&&sqlite3_step(del.p)==SQLITE_DONE;}
 for(int i=0;i<ame::kFundCount&&ok;++i){Q ins;ok=prep(s->db,ins,"INSERT INTO am_pay_fund_rates(period,staff,fund,employee_bp,employer_bp) VALUES(?,?,?,?,?)")&&b(ins,1,p)&&n(ins,2,staff)&&n(ins,3,i)&&n(ins,4,input.funds[i].employeeBp)&&n(ins,5,input.funds[i].employerBp)&&sqlite3_step(ins.p)==SQLITE_DONE;}
 if(ok)ok=audit(s,L"profile_saved",L"Персонални осигурителни ставки; сумата по фондове е потвърдена от потребителя.");
 if(ok&&sql(s->db,"COMMIT"))return true;
 sql(s->db,"ROLLBACK");err=L"Неуспешно записване на персоналния профил.";return false;
}
bool apply(State* s,std::wstring& err){
 auto p=get(s->period);int staff=selectedStaff(s);if(!validPeriod(p)||!staff){err=L"Невалидни данни.";return false;}
 if(locked(s,p)){err=L"Годината е заключена.";return false;}
 ame::PayrollInput input;ame::PayrollResult result;
 if(!preview(s,input,result,err))return false;
 // Ensure that the rates applied are actually the saved/confirmed profile,
 // never unreviewed modifications left in edit controls.
 Q profile;bool match=prep(s->db,profile,"SELECT insurance_base,tax_relief,tax_bp,reviewer FROM am_pay_fund_profile WHERE period=? AND staff=?")&&b(profile,1,p)&&n(profile,2,staff)&&sqlite3_step(profile.p)==SQLITE_ROW;
 if(!match||sqlite3_column_int64(profile.p,0)!=input.insuranceBase||sqlite3_column_int64(profile.p,1)!=input.taxRelief||sqlite3_column_int(profile.p,2)!=input.incomeTaxBp||txt(profile.p,3)!=get(s->reviewer)){
  err=L"Първо запазете проверения профил с текущите стойности.";return false;
 }
 Q stored;if(!prep(s->db,stored,"SELECT fund,employee_bp,employer_bp FROM am_pay_fund_rates WHERE period=? AND staff=?")||!b(stored,1,p)||!n(stored,2,staff)){err=L"Липсват фондовете.";return false;}
 int count=0;while(sqlite3_step(stored.p)==SQLITE_ROW){int i=sqlite3_column_int(stored.p,0);if(i<0||i>=ame::kFundCount||sqlite3_column_int(stored.p,1)!=input.funds[i].employeeBp||sqlite3_column_int(stored.p,2)!=input.funds[i].employerBp){err=L"Ставките не съвпадат със запазените. Запазете профила отново.";return false;}++count;}
 if(count!=ame::kFundCount){err=L"Профилът е непълен.";return false;}
 if(!sql(s->db,"BEGIN IMMEDIATE")){err=L"Базата е заета.";return false;}
 Q q;bool ok=prep(s->db,q,"UPDATE am_pay_work SET employee_ins=?,employer_ins=?,tax=? WHERE period=? AND staff=?")&&n(q,1,result.employeeInsurance)&&n(q,2,result.employerInsurance)&&n(q,3,result.incomeTax)&&b(q,4,p)&&n(q,5,staff)&&sqlite3_step(q.p)==SQLITE_DONE&&sqlite3_changes(s->db.p)==1;
 if(ok){Q del;ok=prep(s->db,del,"DELETE FROM am_pay_fund_result WHERE period=? AND staff=?")&&b(del,1,p)&&n(del,2,staff)&&sqlite3_step(del.p)==SQLITE_DONE;}
 for(int i=0;i<ame::kFundCount&&ok;++i){Q ins;ok=prep(s->db,ins,"INSERT INTO am_pay_fund_result(period,staff,fund,employee_amount,employer_amount,calculated_at) VALUES(?,?,?,?,?,datetime('now'))")&&b(ins,1,p)&&n(ins,2,staff)&&n(ins,3,i)&&n(ins,4,result.employeeByFund[i])&&n(ins,5,result.employerByFund[i])&&sqlite3_step(ins.p)==SQLITE_DONE;}
 if(ok)ok=audit(s,L"applied",L"Записан автоматичен разчет по фондове: нето "+ame::FormatMoney(result.net));
 if(ok&&sql(s->db,"COMMIT"))return true;
 sql(s->db,"ROLLBACK");err=L"Неуспешно прилагане; всички промени са отменени.";return false;
}

bool loadCategory(State* s,std::wstring& err){
 auto category=get(s->insuredKind),period=get(s->period);
 if(category.empty()||!validPeriod(period)){err=L"Въведете валиден месец и код за вид осигурен.";return false;}
 Q newest;if(!prep(s->db,newest,"SELECT from_period FROM am_pay_category_rates WHERE insured_type=? AND from_period<=? GROUP BY from_period ORDER BY from_period DESC LIMIT 1")||!b(newest,1,category)||!b(newest,2,period)||sqlite3_step(newest.p)!=SQLITE_ROW){err=L"Няма запазени ставки за тази категория и период.";return false;}
 auto sourcePeriod=txt(newest.p,0);
 Q q;if(!prep(s->db,q,"SELECT fund,employee_bp,employer_bp FROM am_pay_category_rates WHERE insured_type=? AND from_period=? ORDER BY fund")||!b(q,1,category)||!b(q,2,sourcePeriod)){err=L"Не може да се прочете профилът.";return false;}
 std::array<int,ame::kFundCount> emp{},employer{};int count=0;
 while(sqlite3_step(q.p)==SQLITE_ROW){int i=sqlite3_column_int(q.p,0);if(i<0||i>=ame::kFundCount||i!=count){err=L"Профилът за категория е непълен.";return false;}emp[i]=sqlite3_column_int(q.p,1);employer[i]=sqlite3_column_int(q.p,2);++count;}
 if(count!=ame::kFundCount){err=L"Няма пълен профил по шест фонда.";return false;}
 for(int i=0;i<ame::kFundCount;++i){set(s->emp[i],ame::FormatMoney(emp[i]));set(s->employer[i],ame::FormatMoney(employer[i]));}
 set(s->out,L"Зареден шаблон за категория "+category+L" от "+sourcePeriod+L". Проверете годината, КИД, служителя, рисковата група, минималния/максималния доход и изключенията, после запазете персоналния профил.");
 return true;
}
bool saveCategory(State* s,std::wstring& err){
 auto category=get(s->insuredKind),period=get(s->period),reviewer=get(s->reviewer);
 if(category.empty()||!validPeriod(period)||reviewer.empty()){err=L"Въведете код за вид осигурен, месец и проверяващ.";return false;}
 if(locked(s,period)){err=L"Годината е заключена.";return false;}
 ame::PayrollInput input;ame::PayrollResult result;
 if(!preview(s,input,result,err))return false;
 if(!sql(s->db,"BEGIN IMMEDIATE")){err=L"Базата е заета.";return false;}
 bool ok=true;
 for(int i=0;i<ame::kFundCount&&ok;++i){Q q;
  ok=prep(s->db,q,"INSERT OR REPLACE INTO am_pay_category_rates(from_period,insured_type,fund,employee_bp,employer_bp,reviewer,updated_at) VALUES(?,?,?,?,?,?,datetime('now'))")&&b(q,1,period)&&b(q,2,category)&&n(q,3,i)&&n(q,4,input.funds[i].employeeBp)&&n(q,5,input.funds[i].employerBp)&&b(q,6,reviewer)&&sqlite3_step(q.p)==SQLITE_DONE;
 }
 if(ok)ok=audit(s,L"category_template_saved",L"Ставки за код "+category+L", приложими от "+period+L"; потвърдени от "+reviewer);
 if(ok&&sql(s->db,"COMMIT"))return true;
 sql(s->db,"ROLLBACK");err=L"Шаблонът за категория не е записан.";return false;
}
void create(HWND h,State* s){s->hwnd=h;s->font=CreateFontW(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,L"Tahoma");
 label(h,s,L"АВТОМАТИЧЕН РАЗЧЕТ ПО ОСИГУРИТЕЛНИ ФОНДОВЕ — СЛЕД ПОТВЪРДЕНИ СТАВКИ",20,12,1070);
 label(h,s,L"Месец",20,46,140);s->period=edit(h,s,20,70,145,current().c_str());
 label(h,s,L"Служител",185,46,440);s->combo=control(h,s,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,185,70,430,400,301);updateStaffList(s);
 label(h,s,L"Проверил ставките",635,46,430);s->reviewer=edit(h,s,635,70,450);
 label(h,s,L"Осигурителна основа",20,114,190);s->base=edit(h,s,20,138,165);
 label(h,s,L"Данъчно облекчение",205,114,190);s->relief=edit(h,s,205,138,170);
 label(h,s,L"ДОД %",395,114,110);s->tax=edit(h,s,395,138,100,L"10.00");
 label(h,s,L"Вид осигурен (код)",515,114,170);s->insuredKind=edit(h,s,515,138,145);
 label(h,s,L"Отраб. дни",680,114,130);s->days=edit(h,s,680,138,110);
 label(h,s,L"НКПД",810,114,115);s->nkpd=edit(h,s,810,138,115);
 label(h,s,L"КИД",945,114,115);s->kid=edit(h,s,945,138,140);
 label(h,s,L"Фонд",20,188,330);label(h,s,L"Лични %",405,188,150);label(h,s,L"Работодател %",595,188,175);
 for(int i=0;i<ame::kFundCount;++i){int y=222+i*49;label(h,s,ame::kFundNames[i],20,y+3,360);s->emp[i]=edit(h,s,400,y,140,L"0.00");s->employer[i]=edit(h,s,590,y,140,L"0.00");}
 label(h,s,L"Ставките за категория могат да се използват повторно от избраната дата (след проверка).",20,531,1050);
 button(h,s,L"Зареди записан профил",302,20,566,220);button(h,s,L"Преглед / изчисли",303,260,566,210);
 button(h,s,L"Запази ставки",304,490,566,180);button(h,s,L"Приложи към ведомост",305,690,566,220);button(h,s,L"Затвори",306,930,566,145);
 button(h,s,L"Зареди ставки за вид осигурен",307,20,606,290);button(h,s,L"Запази ставки за вид осигурен",308,330,606,300);
 s->out=control(h,s,L"EDIT",L"",WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL,20,651,1050,110,350);
 load(s);
}
LRESULT CALLBACK wndproc(HWND h,UINT msg,WPARAM w,LPARAM l){
 State* s=(State*)GetWindowLongPtrW(h,GWLP_USERDATA);
 if(msg==WM_NCCREATE){s=(State*)((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)s);}
 if(!s)return DefWindowProcW(h,msg,w,l);
 if(msg==WM_CREATE){create(h,s);return 0;}
 if(msg==WM_COMMAND){int id=LOWORD(w);if(id==301&&HIWORD(w)==CBN_SELCHANGE){load(s);return 0;}
  if(id==302){load(s);return 0;}
  if(id==303){showPreview(s);return 0;}
  if(id==304){std::wstring err;bool ok=saveProfile(s,err);info(h,ok?L"Ставките са записани за този служител и месец. Не са автоматично потвърдени от НАП.":err,ok);if(ok)showPreview(s);return 0;}
  if(id==305){if(MessageBoxW(h,L"Това ще замени ръчно въведените лични/работодателски осигуровки и данък за избрания служител и месец. Потвърждавате ли, че профилът е проверен?",L"AME — запис във ведомост",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES)return 0;std::wstring err;bool ok=apply(s,err);info(h,ok?L"Автоматичните суми са записани във ведомостта и по отделни фондове.":err,ok);if(ok)showPreview(s);return 0;}
  if(id==307){std::wstring err;bool ok=loadCategory(s,err);if(!ok)info(h,err);return 0;}
  if(id==308){if(MessageBoxW(h,L"Ще запазите тези проценти за повторна употреба за всички служители с този код на вид осигурен от избрания месец нататък. Потвърдете, че ставките са проверени за съответната категория.",L"AME — шаблон осигуровки",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES)return 0;std::wstring err;bool ok=saveCategory(s,err);info(h,ok?L"Ставките за категория са записани с период на влизане в сила и проверяващ.":err,ok);return 0;}
  if(id==306){DestroyWindow(h);return 0;}
 }
 if(msg==WM_CLOSE){DestroyWindow(h);return 0;}
 if(msg==WM_DESTROY){if(s->font)DeleteObject(s->font);return 0;}
 return DefWindowProcW(h,msg,w,l);
}
} // namespace

void ShowAMEFundCalculator(HWND parent,int companyId,const std::wstring& companyName){
 if(companyId<=0)return;State s;s.company=companyId;s.companyName=companyName;
 if(!openCompany(s.db,companyId)){info(parent,L"Не може да се отвори фирмената база.");return;}
 WNDCLASSW wc={};wc.lpfnWndProc=wndproc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.lpszClassName=L"AMEFunds060";RegisterClassW(&wc);
 std::wstring caption=L"AME — осигуровки по фондове — "+companyName;
 HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,caption.c_str(),WS_CAPTION|WS_SYSMENU|WS_POPUP,CW_USEDEFAULT,CW_USEDEFAULT,1100,815,parent,nullptr,wc.hInstance,&s);
 if(!h)return;EnableWindow(parent,FALSE);ShowWindow(h,SW_SHOW);MSG m;
 while(IsWindow(h)){int status=GetMessageW(&m,nullptr,0,0);if(status<=0){if(status==0)PostQuitMessage((int)m.wParam);break;}if(!IsDialogMessageW(h,&m)){TranslateMessage(&m);DispatchMessageW(&m);}}
 if(IsWindow(h))DestroyWindow(h);EnableWindow(parent,TRUE);SetActiveWindow(parent);
}
