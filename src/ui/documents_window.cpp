#include "documents_window.h"
#include "extra_modules.h"
#include "../data/document_store.h"
#include "../data/ame_calculations.h"
#include "../data/chart_template.h"
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <sqlite3.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace {
struct Sql { sqlite3_stmt* p=nullptr; ~Sql(){if(p)sqlite3_finalize(p);} };
struct Account {int id;std::wstring code,name;};
struct Partner {int id;std::wstring name,eik;};
struct View {
    sqlite3* db=nullptr; ~View(){if(db)sqlite3_close(db);}
    HWND window=nullptr;
    HWND mode[3]={}, number=nullptr,date=nullptr,registered=nullptr,deal=nullptr,explanation=nullptr;
    HWND partner=nullptr, base=nullptr,rate=nullptr,vat=nullptr,gross=nullptr,basis=nullptr;
    HWND payment=nullptr,baseAccount=nullptr,addAccount=nullptr,analytical=nullptr,quantity=nullptr,lineAmount=nullptr;
    HWND debitList=nullptr,creditList=nullptr,totalLabel=nullptr,status=nullptr;
    HFONT font=nullptr,bold=nullptr;
    int companyId=0,kind=0;
    std::wstring companyName;
    std::vector<Account> accounts;
    std::vector<Partner> partners;
    std::vector<ame_doc::Line> lines;
    std::int64_t entryId=0;
    bool dirty=false,loading=false,fromGross=false,proposalOutdated=false;
};
std::string u8(const std::wstring& s){
    if(s.empty())return "";
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);
    if(n<=0)return "";
    std::string out((size_t)n,'\0');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),(int)s.size(),&out[0],n,nullptr,nullptr);
    return out;
}
std::wstring w8(const std::string& s){
    if(s.empty())return L"";
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);
    if(n<=0)return L"";
    std::wstring out((size_t)n,L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),&out[0],n);
    return out;
}
std::wstring read(HWND h){if(!h)return L"";int n=GetWindowTextLengthW(h);std::vector<wchar_t> b((size_t)n+1);GetWindowTextW(h,b.data(),n+1);return b.data();}
void set(HWND h,const std::wstring& s){if(h)SetWindowTextW(h,s.c_str());}
std::wstring trim(std::wstring s){auto a=s.find_first_not_of(L" \r\t\n");if(a==std::wstring::npos)return L"";auto b=s.find_last_not_of(L" \r\t\n");return s.substr(a,b-a+1);}
void warn(HWND h,const std::wstring& s,bool success=false){MessageBoxW(h,s.c_str(),L"AME — Документи",MB_OK|(success?MB_ICONINFORMATION:MB_ICONWARNING));}
bool prepare(sqlite3* d,Sql& s,const char* sql){return sqlite3_prepare_v2(d,sql,-1,&s.p,nullptr)==SQLITE_OK;}
std::wstring txt(sqlite3_stmt* p,int i){auto t=sqlite3_column_text16(p,i);return t?static_cast<const wchar_t*>(t):L"";}
std::wstring now(){SYSTEMTIME t;GetLocalTime(&t);wchar_t b[24];swprintf_s(b,24,L"%04d-%02d-%02d",t.wYear,t.wMonth,t.wDay);return b;}
std::wstring periodDate(sqlite3* db){
    Sql q;std::wstring n=now();
    if(prepare(db,q,"SELECT value FROM am_context WHERE key='period' LIMIT 1")&&sqlite3_step(q.p)==SQLITE_ROW){
        auto p=txt(q.p,0);
        if(p.size()==7 && p[4]==L'-' && ame_doc::ValidDate(u8(p+L"-01")))
            return p==n.substr(0,7)?n:p+L"-01";
    }
    return n;
}
std::wstring dbPath(int company){
    wchar_t buf[MAX_PATH]={};
    if(company<=0 || FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE,nullptr,SHGFP_TYPE_CURRENT,buf)))return L"";
    auto path=std::wstring(buf)+L"\\AME";CreateDirectoryW(path.c_str(),nullptr);
    path+=L"\\companies";CreateDirectoryW(path.c_str(),nullptr);
    return path+L"\\company_"+std::to_wstring(company)+L".db";
}
HWND c(HWND h,View* s,const wchar_t* cls,const wchar_t* val,DWORD style,int x,int y,int w,int ht,int id=0){
    HWND o=CreateWindowExW(0,cls,val,WS_CHILD|WS_VISIBLE|style,x,y,w,ht,h,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
    SendMessageW(o,WM_SETFONT,(WPARAM)s->font,TRUE);return o;
}
void lbl(HWND h,View* s,const wchar_t* text,int x,int y,int w=130){c(h,s,L"STATIC",text,0,x,y,w,21);}
HWND edit(HWND h,View* s,int x,int y,int w,int id=0,bool readonly=false){return c(h,s,L"EDIT",L"",WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL|(readonly?ES_READONLY:0),x,y,w,26,id);}
HWND combo(HWND h,View* s,int x,int y,int w,int id){return c(h,s,L"COMBOBOX",L"",WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,x,y,w,260,id);}
void btn(HWND h,View* s,const wchar_t* label,int id,int x,int y,int w,int ht=32){c(h,s,L"BUTTON",label,WS_TABSTOP|BS_PUSHBUTTON,x,y,w,ht,id);}
void col(HWND table,int i,const wchar_t* label,int width){LVCOLUMNW cc={};cc.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;cc.pszText=const_cast<wchar_t*>(label);cc.cx=width;cc.iSubItem=i;ListView_InsertColumn(table,i,&cc);}
int selected(HWND comboH){return (int)SendMessageW(comboH,CB_GETCURSEL,0,0);}
int accountIdAt(View* s,int index){return index>=0&&index<(int)s->accounts.size()?s->accounts[(size_t)index].id:0;}
int accountByCode(View* s,const wchar_t* code){for(const auto& a:s->accounts)if(a.code==code)return a.id;return 0;}
int accountIndexById(View* s,int id){for(int i=0;i<(int)s->accounts.size();++i)if(s->accounts[(size_t)i].id==id)return i;return -1;}
int partnerAt(View* s){int i=selected(s->partner);return i>=0&&i<(int)s->partners.size()?s->partners[(size_t)i].id:0;}
void choosePartner(View* s,int id){int index=0;for(int i=0;i<(int)s->partners.size();++i)if(s->partners[(size_t)i].id==id){index=i;break;}SendMessageW(s->partner,CB_SETCURSEL,index,0);}
void fillAccounts(View* s){
    s->accounts.clear();SendMessageW(s->baseAccount,CB_RESETCONTENT,0,0);SendMessageW(s->addAccount,CB_RESETCONTENT,0,0);
    Sql q;
    if(!prepare(s->db,q,"SELECT id,code,name FROM accounts WHERE active=1 ORDER BY code"))return;
    while(sqlite3_step(q.p)==SQLITE_ROW){
        Account a={sqlite3_column_int(q.p,0),txt(q.p,1),txt(q.p,2)};
        s->accounts.push_back(a);
        auto text=a.code+L"  "+a.name;
        SendMessageW(s->baseAccount,CB_ADDSTRING,0,(LPARAM)text.c_str());
        SendMessageW(s->addAccount,CB_ADDSTRING,0,(LPARAM)text.c_str());
    }
}
void fillPartners(View* s,int choose=0){
    SendMessageW(s->partner,CB_RESETCONTENT,0,0);s->partners.clear();
    s->partners.push_back({0,L"— Изберете контрагент —",L""});
    SendMessageW(s->partner,CB_ADDSTRING,0,(LPARAM)L"— Изберете контрагент —");
    Sql q;
    if(prepare(s->db,q,"SELECT id,name,eik FROM partners ORDER BY name"))while(sqlite3_step(q.p)==SQLITE_ROW){
        Partner p={sqlite3_column_int(q.p,0),txt(q.p,1),txt(q.p,2)};
        s->partners.push_back(p);
        auto text=p.name+(p.eik.empty()?L"":L"  |  ЕИК "+p.eik);
        SendMessageW(s->partner,CB_ADDSTRING,0,(LPARAM)text.c_str());
    }
    choosePartner(s,choose);
}
void status(View* s,const std::wstring& message){set(s->status,message);}
void totals(View* s){
    std::int64_t dr=0,cr=0;
    for(const auto& l:s->lines) {if(l.debit)dr+=l.amount;else cr+=l.amount;}
    auto t=L"Дебит: "+ame::FormatMoney(dr)+L"     Кредит: "+ame::FormatMoney(cr);
    t+=dr==cr&&dr>0?L"     |  РАВНИ":L"     |  НЕРАВНИ";
    set(s->totalLabel,t);
}
void drawList(View* s){
    ListView_DeleteAllItems(s->debitList);ListView_DeleteAllItems(s->creditList);
    int nD=0,nC=0;
    for(size_t i=0;i<s->lines.size();++i){
        const auto& l=s->lines[i];HWND table=l.debit?s->debitList:s->creditList;
        int row=l.debit?nD++:nC++;
        std::wstring code=L"?",name=L"?";
        for(const auto& a:s->accounts)if(a.id==l.accountId){code=a.code;name=a.name;break;}
        LVITEMW item={};item.mask=LVIF_TEXT|LVIF_PARAM;item.iItem=row;item.lParam=(LPARAM)i;
        item.pszText=const_cast<wchar_t*>(code.c_str());ListView_InsertItem(table,&item);
        std::wstring cells[]={name,w8(l.analytical),w8(l.quantity),ame::FormatMoney(l.amount)};
        for(int j=0;j<4;++j)ListView_SetItemText(table,row,j+1,const_cast<wchar_t*>(cells[j].c_str()));
    }
    totals(s);
}
bool discard(View* s){return !s->dirty||MessageBoxW(s->window,L"Има незаписани промени. Да бъдат ли отхвърлени?",L"AME",MB_YESNO|MB_ICONQUESTION)==IDYES;}
void updateVAT(View* s){
    if(s->kind==0){set(s->vat,L"0.00");set(s->gross,L"0.00");return;}
    ame::Money base=0,tax=0,gross=0;
    int rate=selected(s->rate)==1?900:selected(s->rate)==2?0:2000;
    if(!ame::ParseMoney(trim(read(s->base)),base)||!ame::VatFromBase(base,rate,tax,gross)){
        set(s->vat,L"—");set(s->gross,L"—");return;
    }
    set(s->vat,ame::FormatMoney(tax));set(s->gross,ame::FormatMoney(gross));
}
void changeMode(View* s,int kind,bool clearLines){
    s->kind=kind;
    if(clearLines)s->fromGross=false;
    for(int k=0;k<3;++k)SendMessageW(s->mode[k],BM_SETCHECK,k==kind?BST_CHECKED:BST_UNCHECKED,0);
    bool invoice=kind!=0;
    for(HWND h:{s->partner,s->base,s->rate,s->basis,s->payment,s->baseAccount})EnableWindow(h,invoice);
    SendMessageW(s->baseAccount,CB_SETCURSEL,accountIndexById(s,accountByCode(s,kind==2?L"703":L"601")),0);
    if(clearLines){s->lines.clear();drawList(s);s->dirty=true;s->proposalOutdated=false;}
    updateVAT(s);
    status(s,kind==0?L"Свободни операции: въведете счетоводни редове по дебит и кредит.":
           L"Покупки / продажби: попълнете документа, изберете ДДС режим и натиснете 'Предложи записи'.");
}
void reset(View* s){
    s->loading=true;s->entryId=0;s->fromGross=false;s->proposalOutdated=false;
    auto d=periodDate(s->db);
    set(s->date,d);set(s->registered,d);
    for(HWND h:{s->number,s->deal,s->explanation,s->base,s->basis,s->analytical,s->quantity,s->lineAmount})set(h,L"");
    set(s->base,L"0.00");set(s->quantity,L"");
    SendMessageW(s->rate,CB_SETCURSEL,0,0);SendMessageW(s->payment,CB_SETCURSEL,0,0);
    s->lines.clear();changeMode(s,0,false);drawList(s);fillPartners(s);
    SendMessageW(s->addAccount,CB_SETCURSEL,(WPARAM)-1,0);
    s->dirty=false;s->loading=false;
}
void suggest(View* s){
    if(s->kind==0){warn(s->window,L"За свободна операция въведете сумите по дебит и кредит.\nСметките могат да бъдат различни и редовете — повече от два.");return;}
    if(!partnerAt(s)){warn(s->window,L"Изберете контрагент. Може да го добавите от бутона 'Контрагенти'.");return;}
    ame::Money base=0,tax=0,total=0;
    int bp=selected(s->rate)==1?900:selected(s->rate)==2?0:2000;
    bool vatOk=ame::ParseMoney(trim(read(s->base)),base)&&base>0;
    if(vatOk){
        if(s->fromGross){
            vatOk=ame::ParseMoney(trim(read(s->vat)),tax) &&
                  ame::VatFromGross(base+tax,bp,total,tax) && total==base;
            // For a gross-derived split, calculate independently to avoid
            // recomputing VAT from the net and introducing a 1-cent error.
            if(vatOk) total=base+tax;
        }else vatOk=ame::VatFromBase(base,bp,tax,total);
    }
    if(!vatOk){
        warn(s->window,L"Въведете положителна данъчна основа.");return;
    }
    if(bp==0&&trim(read(s->basis)).empty()){
        warn(s->window,L"За 0% ДДС попълнете основанието за данъчния режим.");return;
    }
    int debitOrCredit=accountIdAt(s,selected(s->baseAccount));
    int taxId=accountByCode(s,s->kind==1?L"4531":L"4532");
    int settlement=accountByCode(s,selected(s->payment)==1?L"501":selected(s->payment)==2?L"503":s->kind==1?L"401":L"411");
    if(!debitOrCredit||!settlement||(tax&&!taxId)){
        warn(s->window,L"Липсва активна сметка за разход/приход, контрагент или ДДС в Сметкоплан.");return;
    }
    if(!s->lines.empty()&&MessageBoxW(s->window,L"Новото предложение ще замени въведените редове. Продължавате ли?",L"AME",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
    s->lines.clear();
    if(s->kind==1){
        s->lines.push_back({debitOrCredit,base,true,"",""});
        if(tax)s->lines.push_back({taxId,tax,true,"",""});
        s->lines.push_back({settlement,total,false,"",""});
    }else{
        s->lines.push_back({settlement,total,true,"",""});
        s->lines.push_back({debitOrCredit,base,false,"",""});
        if(tax)s->lines.push_back({taxId,tax,false,"",""});
    }
    drawList(s);s->dirty=true;s->proposalOutdated=false;
    status(s,L"Предложението е само начална счетоводна статия. Проверете сметките и данъчния режим преди F5.");
}
void addLine(View* s,bool debit){
    auto id=accountIdAt(s,selected(s->addAccount));ame::Money amount=0;
    if(!id||!ame::ParseMoney(trim(read(s->lineAmount)),amount)||amount<=0){
        warn(s->window,L"Изберете активна сметка и положителна сума.");return;
    }
    auto qty=trim(read(s->quantity));
    if(qty.size()>30||qty.find_first_not_of(L"0123456789.,")!=std::wstring::npos){warn(s->window,L"Невалидно количество.");return;}
    s->lines.push_back({id,amount,debit,u8(trim(read(s->analytical))),u8(qty)});
    drawList(s);set(s->lineAmount,L"");set(s->quantity,L"");set(s->analytical,L"");s->dirty=true;
}
void removeLine(View* s){
    for(HWND table:{s->debitList,s->creditList}){
        int sel=ListView_GetNextItem(table,-1,LVNI_SELECTED);
        if(sel<0)continue;
        LVITEMW it={};it.mask=LVIF_PARAM;it.iItem=sel;
        if(ListView_GetItem(table,&it) && it.lParam>=0 && (size_t)it.lParam<s->lines.size()){
            s->lines.erase(s->lines.begin()+it.lParam);drawList(s);s->dirty=true;return;
        }
    }
    warn(s->window,L"Изберете ред от дебитната или кредитната таблица.");
}
bool load(View* s,std::int64_t id){
    ame_doc::Document d;std::string error;
    if(!ame_doc::Load(s->db,id,d,error)){warn(s->window,L"Документът не може да се отвори: "+w8(error));return false;}
    s->loading=true;s->entryId=id;s->fromGross=d.vatMethod==1;
    set(s->number,w8(d.number));set(s->date,w8(d.date));set(s->registered,w8(d.registerDate));
    set(s->explanation,w8(d.explanation));set(s->deal,w8(d.deal));set(s->base,ame::FormatMoney(d.taxBase));set(s->basis,w8(d.legalBasis));
    SendMessageW(s->rate,CB_SETCURSEL,d.vatBp==900?1:d.vatBp==0?2:0,0);
    choosePartner(s,d.partnerId);changeMode(s,d.kind,false);
    set(s->vat,ame::FormatMoney(d.vat));set(s->gross,ame::FormatMoney(d.taxBase+d.vat));
    // Infer existing payment type/base account from the actual journal lines.
    // Do not silently replace a customized account with a template default.
    if(d.kind!=0){
        const bool sale=d.kind==2;
        int paymentType=0,baseId=0;
        for(const auto& line:d.lines){
            std::wstring code;
            for(const auto& account:s->accounts)if(account.id==line.accountId){code=account.code;break;}
            if(line.debit==sale){
                if(code==L"501")paymentType=1;
                else if(code==L"503")paymentType=2;
            }else if(code!=L"4531"&&code!=L"4532"&&baseId==0)baseId=line.accountId;
        }
        SendMessageW(s->payment,CB_SETCURSEL,paymentType,0);
        if(baseId)SendMessageW(s->baseAccount,CB_SETCURSEL,accountIndexById(s,baseId),0);
    }
    s->lines=d.lines;drawList(s);
    s->loading=false;s->dirty=false;s->proposalOutdated=false;
    status(s,L"Отворен документ #"+std::to_wstring(id)+L". Промените се записват в същия счетоводен запис.");
    return true;
}
bool collect(View* s,ame_doc::Document& d){
    d={};d.entryId=s->entryId;d.kind=s->kind;d.partnerId=partnerAt(s);
    d.number=u8(trim(read(s->number)));d.date=u8(trim(read(s->date)));d.registerDate=u8(trim(read(s->registered)));
    d.explanation=u8(trim(read(s->explanation)));d.deal=u8(trim(read(s->deal)));d.legalBasis=u8(trim(read(s->basis)));
    d.lines=s->lines;
    if(s->kind!=0){
        d.vatBp=selected(s->rate)==1?900:selected(s->rate)==2?0:2000;
        d.vatMethod=s->fromGross?1:0;
        if(!ame::ParseMoney(trim(read(s->base)),d.taxBase)||d.taxBase<=0){warn(s->window,L"Въведете валидна данъчна основа.");return false;}
        ame::Money total=0;
        if(s->fromGross){
            if(!ame::ParseMoney(trim(read(s->vat)),d.vat)){
                warn(s->window,L"Невалиден ДДС при обратно изчисление.");return false;
            }
        }else if(!ame::VatFromBase(d.taxBase,d.vatBp,d.vat,total)){
            warn(s->window,L"Невалиден ДДС режим или сума.");return false;
        }
    }
    return true;
}
void save(View* s){
    if(s->proposalOutdated){
        warn(s->window,L"Променени са сума, ДДС или начин на плащане.\nНатиснете 'Предложи записи', за да обновите счетоводните редове преди запис.");return;
    }
    ame_doc::Document d;if(!collect(s,d))return;
    std::int64_t dr=0,cr=0;std::string validation;
    if(!ame_doc::Balanced(d.lines,dr,cr,validation)){warn(s->window,L"Невалидна операция: "+w8(validation));return;}
    auto question=L"Да се осчетоводи документът?\n\nДебит: "+ame::FormatMoney(dr)+L"\nКредит: "+ame::FormatMoney(cr)+
       L"\n\nПроверете избраните сметки, контрагент и ДДС режим.";
    if(MessageBoxW(s->window,question.c_str(),L"AME — потвърждение",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
    if(!ame_doc::Save(s->db,d,validation)){
        warn(s->window,L"Неуспешен запис: "+w8(validation)+L"\n\nПроверете за дублиран документ или приключен период.");return;
    }
    s->entryId=d.entryId;s->dirty=false;
    status(s,L"Осчетоводено: #"+std::to_wstring(d.entryId)+L". Документът вече е видим в Опис и счетоводните справки.");
    warn(s->window,L"Документът е осчетоводен и записан в журнала.\nНАП файлове не се генерират автоматично от този екран.",true);
}
void reverseVAT(View* s){
    if(s->kind==0){warn(s->window,L"Изберете Покупки или Продажби.");return;}
    ame::Money gross=0,base=0,vat=0;
    if(!ame::ParseMoney(trim(read(s->base)),gross)||gross<=0){warn(s->window,L"За обратно изчисляване въведете в поле 'Основа / общо' сумата С ДДС.");return;}
    int bp=selected(s->rate)==1?900:selected(s->rate)==2?0:2000;
    if(!ame::VatFromGross(gross,bp,base,vat)){warn(s->window,L"Невалидна сума.");return;}
    s->loading=true;
    set(s->base,ame::FormatMoney(base));set(s->vat,ame::FormatMoney(vat));set(s->gross,ame::FormatMoney(gross));
    s->fromGross=true;s->loading=false;s->dirty=true;
    if(!s->lines.empty())s->proposalOutdated=true;
    status(s,L"Обратно изчисление от общата сума: основа "+ame::FormatMoney(base)+L", ДДС "+ame::FormatMoney(vat)+L". Прегледайте и предложете нови записи.");
}
LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
    View* s=(View*)GetWindowLongPtrW(h,GWLP_USERDATA);
    if(m==WM_NCCREATE){s=(View*)((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)s);s->window=h;}
    if(!s)return DefWindowProcW(h,m,w,l);
    switch(m){
    case WM_CREATE:{
        s->font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        s->bold=CreateFontW(-19,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        lbl(h,s,L"ВИД ОПЕРАЦИЯ",18,8,155);
        const wchar_t* names[]={L"Свободни операции",L"Покупки",L"Продажби"};
        for(int i=0;i<3;++i)s->mode[i]=c(h,s,L"BUTTON",names[i],WS_TABSTOP|BS_AUTORADIOBUTTON|(i==0?WS_GROUP:0),165+i*191,5,183,31,2001+i);
        lbl(h,s,L"Документ №",18,51,145);s->number=edit(h,s,18,74,150,2101);
        lbl(h,s,L"Дата (ГГГГ-ММ-ДД)",181,51,178);s->date=edit(h,s,181,74,153,2102);
        lbl(h,s,L"Дата регистр.",347,51,170);s->registered=edit(h,s,347,74,153,2103);
        lbl(h,s,L"Контрагент",514,51,160);s->partner=combo(h,s,514,74,350,2104);
        btn(h,s,L"Контрагенти...",2105,875,72,162,30);
        lbl(h,s,L"Обяснение / основание",18,112,365);s->explanation=edit(h,s,18,135,557,2110);
        lbl(h,s,L"Сделка / реф. №",588,112,260);s->deal=edit(h,s,588,135,449,2111);
        lbl(h,s,L"Данъчна основа / общо*",18,176,220);s->base=edit(h,s,18,200,162,2120);
        lbl(h,s,L"ДДС ставка",194,176,134);s->rate=combo(h,s,194,200,130,2121);
        for(const wchar_t* t:{L"20%",L"9%",L"0%"})SendMessageW(s->rate,CB_ADDSTRING,0,(LPARAM)t);
        lbl(h,s,L"ДДС",338,176,130);s->vat=edit(h,s,338,200,122,0,true);
        lbl(h,s,L"Обща сума",474,176,142);s->gross=edit(h,s,474,200,126,0,true);
        btn(h,s,L"От общо с ДДС",2122,618,197,170,30);
        btn(h,s,L"Предложи записи",2123,802,197,235,30);
        lbl(h,s,L"Разходна / приходна сметка",18,234,243);s->baseAccount=combo(h,s,18,256,400,2124);
        lbl(h,s,L"Начин на плащане",430,234,210);s->payment=combo(h,s,430,256,210,2125);
        for(const wchar_t* t:{L"Неплатено (401/411)",L"В брой (501)",L"По банка (503)"})SendMessageW(s->payment,CB_ADDSTRING,0,(LPARAM)t);
        lbl(h,s,L"Основание при 0% ДДС",655,234,382);s->basis=edit(h,s,655,256,382,2126);
        HWND hd= c(h,s,L"STATIC",L"ДЕБИТ",0,18,291,270,26);
        HWND hc= c(h,s,L"STATIC",L"КРЕДИТ",0,532,291,270,26);
        SendMessageW(hd,WM_SETFONT,(WPARAM)s->bold,TRUE);SendMessageW(hc,WM_SETFONT,(WPARAM)s->bold,TRUE);
        s->debitList=c(h,s,WC_LISTVIEWW,L"",WS_TABSTOP|WS_BORDER|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,18,319,500,198,2201);
        s->creditList=c(h,s,WC_LISTVIEWW,L"",WS_TABSTOP|WS_BORDER|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,532,319,505,198,2202);
        for(HWND table:{s->debitList,s->creditList}){
            ListView_SetExtendedListViewStyle(table,LVS_EX_GRIDLINES|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
            col(table,0,L"Сметка",66);col(table,1,L"Наименование",177);col(table,2,L"Партида",85);col(table,3,L"Кол.",65);col(table,4,L"Сума",100);
        }
        lbl(h,s,L"Добавяне на ред: сметка",18,524,260);s->addAccount=combo(h,s,18,548,355,2301);
        lbl(h,s,L"Партида",385,524,130);s->analytical=edit(h,s,385,548,150,2302);
        lbl(h,s,L"Количество",547,524,100);s->quantity=edit(h,s,547,548,100,2303);
        lbl(h,s,L"Сума",660,524,113);s->lineAmount=edit(h,s,660,548,110,2304);
        btn(h,s,L"+ Дебит",2305,784,547,112,30);btn(h,s,L"+ Кредит",2306,903,547,134,30);
        btn(h,s,L"Премахни избран ред",2307,18,591,190,33);
        s->totalLabel=c(h,s,L"STATIC",L"Дебит: 0.00   Кредит: 0.00",0,225,592,590,28);
        btn(h,s,L"Нова операция",2401,18,628,155,35);
        btn(h,s,L"F8  В брой",2404,438,628,168,35);
        btn(h,s,L"F5  Осчетоводи",2402,622,628,190,35);
        btn(h,s,L"F7  Изход",2403,827,628,210,35);
        s->status=c(h,s,L"STATIC",L"",0,18,675,1020,27);
        fillAccounts(s);reset(s);
        RegisterHotKey(h,1,0,VK_F5);RegisterHotKey(h,2,0,VK_F7);RegisterHotKey(h,3,0,VK_F8);
        return 0;
    }
    case WM_COMMAND:{
        int id=LOWORD(w);
        if(s->loading)return 0;
        if(id>=2001&&id<=2003&&HIWORD(w)==BN_CLICKED){
            int newKind=id-2001;
            if(s->kind!=newKind && discard(s)){changeMode(s,newKind,true);s->dirty=true;}
            else if(s->kind!=newKind)SendMessageW(s->mode[s->kind],BM_SETCHECK,BST_CHECKED,0);
            return 0;
        }
        if(id==2105){int partner=partnerAt(s);ShowAMEDirectory(h,s->companyId,s->companyName,0);fillPartners(s,partner);return 0;}
        if(id==2122){reverseVAT(s);return 0;}
        if(id==2123){suggest(s);return 0;}
        if(id==2305||id==2306){addLine(s,id==2305);return 0;}
        if(id==2307){removeLine(s);return 0;}
        if(id==2401){if(discard(s))reset(s);return 0;}
        if(id==2402){save(s);return 0;}
        if(id==2403){SendMessageW(h,WM_CLOSE,0,0);return 0;}
        if(id==2404){
            if(s->kind==0){warn(h,L"F8 В брой е налично в Покупки и Продажби.");return 0;}
            SendMessageW(s->payment,CB_SETCURSEL,1,0);s->dirty=true;
            if(!s->lines.empty())s->proposalOutdated=true;
            status(s,L"Плащане в брой (501). Натиснете 'Предложи записи', за да приложите новите сметки.");
            return 0;
        }
        if(HIWORD(w)==EN_CHANGE||HIWORD(w)==CBN_SELCHANGE){
            if(id==2120||id==2121){s->fromGross=false;updateVAT(s);}
            if((id==2120||id==2121||id==2124||id==2125)&&!s->lines.empty()&&s->kind!=0){
                s->proposalOutdated=true;
                status(s,L"Променени са параметри на документа. Натиснете 'Предложи записи' преди осчетоводяване.");
            }
            if(id>=2101&&id<=2304)s->dirty=true;
        }
        return 0;
    }
    case WM_HOTKEY:if(w==1)save(s);else if(w==2)SendMessageW(h,WM_CLOSE,0,0);else if(w==3)SendMessageW(h,WM_COMMAND,2404,0);return 0;
    case WM_CLOSE:if(discard(s))DestroyWindow(h);return 0;
    case WM_DESTROY:UnregisterHotKey(h,1);UnregisterHotKey(h,2);UnregisterHotKey(h,3);if(s->font)DeleteObject(s->font);if(s->bold)DeleteObject(s->bold);return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
}
void ShowDocumentsWindow(HWND parent,int companyId,const std::wstring& companyName,std::int64_t initialEntryId){
    View s;s.companyId=companyId;s.companyName=companyName;
    auto path=dbPath(companyId);
    if(path.empty()||sqlite3_open16(path.c_str(),&s.db)!=SQLITE_OK){warn(parent,L"Не може да се отвори счетоводната база.");return;}
    sqlite3_busy_timeout(s.db,5000);
    std::string err;
    if(!EnsureChartTemplate(s.db)||!ame_doc::EnsureSchema(s.db,err)){warn(parent,L"Не може да се инициализира Документи: "+w8(err));return;}
    INITCOMMONCONTROLSEX init={sizeof(init),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&init);
    WNDCLASSW cls={};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    cls.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);cls.lpszClassName=L"AME_Documents_Workflow_070";RegisterClassW(&cls);
    DWORD style=WS_CAPTION|WS_SYSMENU|WS_POPUP;
    RECT r={0,0,1055,707};AdjustWindowRectEx(&r,style,FALSE,WS_EX_DLGMODALFRAME);
    auto title=L"Документи — "+companyName;
    HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,cls.lpszClassName,title.c_str(),style,CW_USEDEFAULT,CW_USEDEFAULT,
                            r.right-r.left,r.bottom-r.top,parent,nullptr,cls.hInstance,&s);
    if(!h){warn(parent,L"Не може да се отвори прозорецът Документи.");return;}
    if(initialEntryId)load(&s,initialEntryId);
    EnableWindow(parent,FALSE);ShowWindow(h,SW_SHOW);
    MSG msg;
    while(IsWindow(h)){
        BOOL ret=GetMessageW(&msg,nullptr,0,0);
        if(ret<=0){if(ret==0)PostQuitMessage((int)msg.wParam);break;}
        if(!IsDialogMessageW(h,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    }
    if(IsWindow(h))DestroyWindow(h);
    EnableWindow(parent,TRUE);SetActiveWindow(parent);
}
