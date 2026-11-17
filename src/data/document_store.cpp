#include "document_store.h"
#include <sqlite3.h>
#include <algorithm>
#include <climits>
#include <limits>
#include <string>
#include <utility>

namespace ame_doc {
namespace {
struct Q {
    sqlite3_stmt* p = nullptr;
    ~Q() { if (p) sqlite3_finalize(p); }
};
bool exec(sqlite3* d, const char* sql) { return sqlite3_exec(d, sql, nullptr, nullptr, nullptr) == SQLITE_OK; }
bool prep(sqlite3* d, Q& q, const char* sql) { return sqlite3_prepare_v2(d, sql, -1, &q.p, nullptr) == SQLITE_OK; }
bool text(Q& q, int i, const std::string& s) { return sqlite3_bind_text(q.p, i, s.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK; }
bool num(Q& q, int i, std::int64_t n) { return sqlite3_bind_int64(q.p, i, n) == SQLITE_OK; }
std::string read(sqlite3_stmt* p, int col) { const unsigned char* s = sqlite3_column_text(p, col); return s ? reinterpret_cast<const char*>(s) : ""; }
bool stepDone(Q& q) { return sqlite3_step(q.p) == SQLITE_DONE; }
std::string sqlerr(sqlite3* d) { return d ? sqlite3_errmsg(d) : "Database is not open"; }
bool exists(sqlite3* d, const char* query, std::int64_t id) {
    Q q; return prep(d,q,query) && num(q,1,id) && sqlite3_step(q.p)==SQLITE_ROW;
}
std::string trim(const std::string& s) { size_t a=s.find_first_not_of(" \t\r\n"), b=s.find_last_not_of(" \t\r\n"); return a==std::string::npos?"":s.substr(a,b-a+1); }
}
bool ValidDate(const std::string& d) {
    if(d.size()!=10 || d[4]!='-' || d[7]!='-') return false;
    for(int i=0;i<10;++i) if(i!=4 && i!=7 && (d[i]<'0'||d[i]>'9'))return false;
    int y=std::stoi(d.substr(0,4)),m=std::stoi(d.substr(5,2)),day=std::stoi(d.substr(8,2));
    static const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(y<1900||y>2200||m<1||m>12)return false;
    int leap=(m==2 && y%4==0 && (y%100!=0||y%400==0))?1:0;
    return day>=1 && day<=days[m-1]+leap;
}
bool Balanced(const std::vector<Line>& lines,std::int64_t& debit,std::int64_t& credit,std::string& problem) {
    debit=0;credit=0;problem.clear();
    if(lines.size()<2 || lines.size()>200) { problem="At least two and at most 200 accounting lines are required.";return false; }
    for(const auto& l:lines) {
        if(l.accountId<=0 || l.amount<=0 || l.amount>9000000000000LL) { problem="Invalid account or nonpositive amount.";return false; }
        auto& total = l.debit ? debit : credit;
        if(total>9000000000000LL-l.amount){problem="Amount exceeds safe limit.";return false;}
        total+=l.amount;
        if(l.analytical.size()>512 || l.quantity.size()>80) {problem="Analytical reference is too long.";return false;}
    }
    if(debit!=credit || debit==0){problem="Debit and credit totals must be equal and nonzero.";return false;}
    // The common self-cancelling mistake should not be silently accepted.
    if(lines.size()==2 && lines[0].accountId==lines[1].accountId &&
       lines[0].debit!=lines[1].debit && lines[0].analytical==lines[1].analytical) {
        problem="Both sides use the same account and analytical reference: no economic change.";return false;
    }
    return true;
}
bool EnsureSchema(sqlite3* db,std::string& err) {
    err.clear();if(!db){err="Database is not open.";return false;}
    // No destructive schema changes. Tables are additive and older invoices remain unchanged.
    const char* sql=
      "PRAGMA foreign_keys=ON;"
      "CREATE TABLE IF NOT EXISTS journal_entries(id INTEGER PRIMARY KEY,date TEXT NOT NULL,document TEXT NOT NULL,description TEXT NOT NULL);"
      "CREATE TABLE IF NOT EXISTS journal_lines(entry_id INTEGER NOT NULL REFERENCES journal_entries(id),"
      "line_no INTEGER NOT NULL,account_id INTEGER NOT NULL REFERENCES accounts(id),"
      "debit INTEGER NOT NULL CHECK(debit>=0),credit INTEGER NOT NULL CHECK(credit>=0),"
      "CHECK((debit>0 AND credit=0) OR (credit>0 AND debit=0)),PRIMARY KEY(entry_id,line_no));"
      "CREATE TABLE IF NOT EXISTS partners(id INTEGER PRIMARY KEY,name TEXT NOT NULL,eik TEXT NOT NULL,vat TEXT NOT NULL,address TEXT NOT NULL);"
      "CREATE TABLE IF NOT EXISTS am_documents("
      "entry_id INTEGER PRIMARY KEY REFERENCES journal_entries(id),"
      "kind INTEGER NOT NULL CHECK(kind IN (0,1,2)),"
      "partner_id INTEGER REFERENCES partners(id),"
      "document_number TEXT NOT NULL, document_date TEXT NOT NULL,"
      "register_date TEXT NOT NULL, deal TEXT NOT NULL,"
      "tax_base INTEGER NOT NULL DEFAULT 0, vat INTEGER NOT NULL DEFAULT 0,"
      "vat_bp INTEGER NOT NULL DEFAULT 0, vat_method INTEGER NOT NULL DEFAULT 0, legal_basis TEXT NOT NULL DEFAULT '',"
      "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, modified_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);"
      "CREATE TABLE IF NOT EXISTS am_document_line_meta("
      "entry_id INTEGER NOT NULL REFERENCES am_documents(entry_id),"
      "line_no INTEGER NOT NULL, analytical TEXT NOT NULL DEFAULT '', quantity TEXT NOT NULL DEFAULT '',"
      "PRIMARY KEY(entry_id,line_no));"
      "CREATE TABLE IF NOT EXISTS am_document_audit("
      "id INTEGER PRIMARY KEY, entry_id INTEGER NOT NULL, action TEXT NOT NULL,"
      "at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, prior_number TEXT NOT NULL,"
      "prior_date TEXT NOT NULL, note TEXT NOT NULL);"
      "CREATE INDEX IF NOT EXISTS am_documents_by_partner ON am_documents(partner_id,document_date);"
      "CREATE UNIQUE INDEX IF NOT EXISTS am_documents_no_duplicate ON"
      " am_documents(kind,partner_id,document_number,substr(document_date,1,4)) WHERE kind IN (1,2);"
      "CREATE TABLE IF NOT EXISTS am_document_write_gate(id INTEGER PRIMARY KEY CHECK(id=1),enabled INTEGER NOT NULL);"
      "INSERT OR IGNORE INTO am_document_write_gate(id,enabled) VALUES(1,0);"
      "CREATE TRIGGER IF NOT EXISTS am_document_guard_journal BEFORE UPDATE ON journal_entries"
      " WHEN EXISTS(SELECT 1 FROM am_documents WHERE entry_id=OLD.id)"
      " AND (SELECT enabled FROM am_document_write_gate WHERE id=1)=0"
      " BEGIN SELECT RAISE(ABORT,'AME: edit via Documents'); END;"
      "CREATE TRIGGER IF NOT EXISTS am_document_guard_line_del BEFORE DELETE ON journal_lines"
      " WHEN EXISTS(SELECT 1 FROM am_documents WHERE entry_id=OLD.entry_id)"
      " AND (SELECT enabled FROM am_document_write_gate WHERE id=1)=0"
      " BEGIN SELECT RAISE(ABORT,'AME: edit via Documents'); END;"
      "CREATE TRIGGER IF NOT EXISTS am_document_guard_line_ins BEFORE INSERT ON journal_lines"
      " WHEN EXISTS(SELECT 1 FROM am_documents WHERE entry_id=NEW.entry_id)"
      " AND (SELECT enabled FROM am_document_write_gate WHERE id=1)=0"
      " BEGIN SELECT RAISE(ABORT,'AME: edit via Documents'); END;";
    if(!exec(db,sql)){err=sqlerr(db);return false;}
    return true;
}
bool IsWorkspaceDocument(sqlite3* db,std::int64_t entryId) {
    return db && exists(db,"SELECT 1 FROM am_documents WHERE entry_id=?",entryId);
}
bool Save(sqlite3* db,Document& doc,std::string& err) {
    err.clear(); if(!db){err="Database is not open.";return false;}
    if(!EnsureSchema(db,err))return false;
    std::int64_t totalDebit=0,totalCredit=0;
    if(!Balanced(doc.lines,totalDebit,totalCredit,err))return false;
    if(doc.kind<0||doc.kind>2 || !ValidDate(doc.date)||!ValidDate(doc.registerDate)||
       trim(doc.number).empty()||trim(doc.explanation).empty()||
       doc.number.size()>96||doc.explanation.size()>2048||doc.deal.size()>256||
       doc.legalBasis.size()>512) {err="Missing or invalid document number, date or explanation.";return false;}
    if(doc.kind!=0) {
        if(doc.partnerId<=0 || doc.taxBase<=0 || doc.vat<0 || (doc.vatBp!=0&&doc.vatBp!=900&&doc.vatBp!=2000)) {
            err="Purchase/sale needs a partner, positive taxable base and valid VAT rate.";return false;
        }
        if(doc.vatBp==0 && trim(doc.legalBasis).empty()) {err="Zero-rated treatment needs a stated legal basis.";return false;}
        if(doc.taxBase>9000000000000LL || doc.vat>9000000000000LL-doc.taxBase){err="Amount exceeds safe range.";return false;}
        if(doc.vatMethod!=0 && doc.vatMethod!=1){err="Unknown VAT rounding method.";return false;}
        std::int64_t expectedVat=0;
        if(doc.vatMethod==0) expectedVat=(doc.taxBase*doc.vatBp+5000)/10000;
        else {
            auto gross=doc.taxBase+doc.vat;
            expectedVat=(gross*doc.vatBp+(10000+doc.vatBp)/2)/(10000+doc.vatBp);
        }
        if(doc.vat!=expectedVat || totalDebit!=doc.taxBase+doc.vat) {err="Document VAT/total does not match journal lines.";return false;}
        if(!exists(db,"SELECT 1 FROM partners WHERE id=?",doc.partnerId)) {err="Selected counterparty does not exist.";return false;}
    } else {
        doc.taxBase=doc.vat=doc.vatBp=doc.vatMethod=0;
    }
    for(const auto& l:doc.lines)
        if(!exists(db,"SELECT 1 FROM accounts WHERE id=? AND active=1",l.accountId)){
            err="An account is missing or inactive.";return false;
        }
    if(!exec(db,"BEGIN IMMEDIATE")){err=sqlerr(db);return false;}
    bool ok=true; const bool edit=doc.entryId!=0;
    do {
        // A locked accounting year must stay locked even if an older build has
        // not yet installed the fiscal-year triggers.
        Q fiscalTable;
        bool hasFiscal=prep(db,fiscalTable,"SELECT 1 FROM sqlite_master WHERE type='table' AND name='am_fiscal_year'") &&
                       sqlite3_step(fiscalTable.p)==SQLITE_ROW;
        if(hasFiscal){
            Q lock;
            ok=prep(db,lock,"SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year=?")&&
                num(lock,1,std::stoi(doc.date.substr(0,4)));
            if(!ok)break;
            if(sqlite3_step(lock.p)==SQLITE_ROW){err="Selected financial year is closed.";ok=false;break;}
            if(edit){
                Q oldLock;
                ok=prep(db,oldLock,"SELECT 1 FROM journal_entries e JOIN am_fiscal_year y ON y.year=CAST(substr(e.date,1,4) AS INTEGER) WHERE e.id=? AND y.locked=1")&&num(oldLock,1,doc.entryId);
                if(!ok)break;
                if(sqlite3_step(oldLock.p)==SQLITE_ROW){err="Original document belongs to a closed year.";ok=false;break;}
            }
        }
        if(!exec(db,"UPDATE am_document_write_gate SET enabled=1 WHERE id=1")){ok=false;break;}
        if(edit) {
            if(!IsWorkspaceDocument(db,doc.entryId)){err="Only entries created in Documents may be edited here.";ok=false;break;}
            if(exists(db,"SELECT 1 FROM invoices WHERE entry=?",doc.entryId) ||
               exists(db,"SELECT 1 FROM payments WHERE entry=?",doc.entryId)) {
                err="Entry is linked to an older invoice or payment.";ok=false;break;
            }
            Q audit;
            ok=prep(db,audit,"INSERT INTO am_document_audit(entry_id,action,prior_number,prior_date,note) "
                              "SELECT entry_id,'EDIT',document_number,document_date,'Previous metadata retained' FROM am_documents WHERE entry_id=?")
                && num(audit,1,doc.entryId)&&stepDone(audit);
            if(!ok)break;
        }
        {
            Q e;
            if(edit) {
                ok=prep(db,e,"UPDATE journal_entries SET date=?,document=?,description=? WHERE id=?")&&
                    text(e,1,doc.date)&&text(e,2,doc.number)&&text(e,3,doc.explanation)&&
                    num(e,4,doc.entryId)&&stepDone(e)&&sqlite3_changes(db)==1;
            }else{
                ok=prep(db,e,"INSERT INTO journal_entries(date,document,description) VALUES(?,?,?)")&&
                    text(e,1,doc.date)&&text(e,2,doc.number)&&text(e,3,doc.explanation)&&stepDone(e);
                if(ok)doc.entryId=sqlite3_last_insert_rowid(db);
            }
        }
        if(!ok)break;
        if(edit){
            Q del1,del2;
            ok=prep(db,del1,"DELETE FROM am_document_line_meta WHERE entry_id=?")&&
                num(del1,1,doc.entryId)&&stepDone(del1)&&
                prep(db,del2,"DELETE FROM journal_lines WHERE entry_id=?")&&
                num(del2,1,doc.entryId)&&stepDone(del2);
            if(!ok)break;
        }
        // Insert metadata before line details: FK checks are immediate.
        {
            Q m;
            const char* sql="INSERT INTO am_documents(entry_id,kind,partner_id,document_number,document_date,register_date,deal,tax_base,vat,vat_bp,vat_method,legal_basis) "
              "VALUES(?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(entry_id) DO UPDATE SET "
              "kind=excluded.kind, partner_id=excluded.partner_id,document_number=excluded.document_number,"
              "document_date=excluded.document_date,register_date=excluded.register_date,deal=excluded.deal,"
              "tax_base=excluded.tax_base,vat=excluded.vat,vat_bp=excluded.vat_bp,vat_method=excluded.vat_method,legal_basis=excluded.legal_basis,"
              "modified_at=CURRENT_TIMESTAMP";
            ok=prep(db,m,sql)&&num(m,1,doc.entryId)&&num(m,2,doc.kind)&&
                (doc.partnerId?num(m,3,doc.partnerId):sqlite3_bind_null(m.p,3)==SQLITE_OK)&&
                text(m,4,doc.number)&&text(m,5,doc.date)&&text(m,6,doc.registerDate)&&
                text(m,7,doc.deal)&&num(m,8,doc.taxBase)&&num(m,9,doc.vat)&&
                num(m,10,doc.vatBp)&&num(m,11,doc.vatMethod)&&text(m,12,doc.legalBasis)&&stepDone(m);
        }
        if(!ok)break;
        for(size_t i=0;i<doc.lines.size();++i){
            const auto& l=doc.lines[i];
            Q line,details;
            ok=prep(db,line,"INSERT INTO journal_lines(entry_id,line_no,account_id,debit,credit) VALUES(?,?,?,?,?)")&&
                num(line,1,doc.entryId)&&num(line,2,static_cast<std::int64_t>(i+1))&&
                num(line,3,l.accountId)&&num(line,4,l.debit?l.amount:0)&&
                num(line,5,l.debit?0:l.amount)&&stepDone(line)&&
                prep(db,details,"INSERT INTO am_document_line_meta(entry_id,line_no,analytical,quantity) VALUES(?,?,?,?)")&&
                num(details,1,doc.entryId)&&num(details,2,static_cast<std::int64_t>(i+1))&&
                text(details,3,l.analytical)&&text(details,4,l.quantity)&&stepDone(details);
            if(!ok)break;
        }
        if(!ok)break;
        if(!exec(db,"UPDATE am_document_write_gate SET enabled=0 WHERE id=1")){ok=false;break;}
        if(!exec(db,"COMMIT")){ok=false;break;}
        return true;
    }while(false);
    if(err.empty())err=sqlerr(db);
    exec(db,"ROLLBACK");
    if(!edit)doc.entryId=0;
    return false;
}
bool Load(sqlite3* db,std::int64_t id,Document& doc,std::string& err){
    err.clear();if(!EnsureSchema(db,err))return false;
    Document d;
    Q h;
    const char* sql="SELECT e.date,e.document,e.description,m.kind,COALESCE(m.partner_id,0),"
      "m.register_date,m.deal,m.tax_base,m.vat,m.vat_bp,m.vat_method,m.legal_basis "
      "FROM journal_entries e JOIN am_documents m ON e.id=m.entry_id WHERE e.id=?";
    if(!prep(db,h,sql)||!num(h,1,id)||sqlite3_step(h.p)!=SQLITE_ROW){err="Document not found in workspace.";return false;}
    d.entryId=id;d.date=read(h.p,0);d.number=read(h.p,1);d.explanation=read(h.p,2);
    d.kind=sqlite3_column_int(h.p,3);d.partnerId=sqlite3_column_int(h.p,4);
    d.registerDate=read(h.p,5);d.deal=read(h.p,6);
    d.taxBase=sqlite3_column_int64(h.p,7);d.vat=sqlite3_column_int64(h.p,8);
    d.vatBp=sqlite3_column_int(h.p,9);d.vatMethod=sqlite3_column_int(h.p,10);d.legalBasis=read(h.p,11);
    Q lines;
    if(!prep(db,lines,"SELECT l.account_id,l.debit,l.credit,COALESCE(m.analytical,''),COALESCE(m.quantity,'') "
       "FROM journal_lines l LEFT JOIN am_document_line_meta m ON m.entry_id=l.entry_id AND m.line_no=l.line_no "
       "WHERE l.entry_id=? ORDER BY l.line_no")||!num(lines,1,id)) {err=sqlerr(db);return false;}
    int rc=0;while((rc=sqlite3_step(lines.p))==SQLITE_ROW){
        Line l;l.accountId=sqlite3_column_int(lines.p,0);auto dr=sqlite3_column_int64(lines.p,1),cr=sqlite3_column_int64(lines.p,2);
        l.debit=dr>0;l.amount=dr>0?dr:cr;l.analytical=read(lines.p,3);l.quantity=read(lines.p,4);d.lines.push_back(l);
    }
    if(rc!=SQLITE_DONE){err=sqlerr(db);return false;}
    doc=std::move(d);return true;
}
} // namespace ame_doc
