#include "../src/data/document_store.h"
#include <sqlite3.h>
#include <cassert>
#include <iostream>
#include <string>
using namespace ame_doc;
static bool sql(sqlite3* db,const char* s){return sqlite3_exec(db,s,nullptr,nullptr,nullptr)==SQLITE_OK;}
static int count(sqlite3* db,const char* s){sqlite3_stmt* q=nullptr;assert(sqlite3_prepare_v2(db,s,-1,&q,nullptr)==SQLITE_OK);assert(sqlite3_step(q)==SQLITE_ROW);int n=sqlite3_column_int(q,0);sqlite3_finalize(q);return n;}
int main(){
 sqlite3* db=nullptr;assert(sqlite3_open(":memory:",&db)==SQLITE_OK);
 assert(sql(db,"PRAGMA foreign_keys=ON;"
    "CREATE TABLE accounts(id INTEGER PRIMARY KEY,code TEXT NOT NULL,active INTEGER NOT NULL);"
    "INSERT INTO accounts VALUES(1,'601',1),(2,'401',1),(3,'4531',1),"
    "(4,'411',1),(5,'703',1),(6,'4532',1),(7,'501',1);"));
 std::string error;assert(EnsureSchema(db,error));
 assert(sql(db,"INSERT INTO partners(id,name,eik,vat,address) VALUES(1,'Supplier','123456789','',''),(2,'Customer','987654321','','');"));
 Document p;p.kind=1;p.partnerId=1;p.number="BUY-001";p.date="2026-11-17";p.registerDate=p.date;
 p.explanation="Purchase";p.taxBase=10000;p.vat=2000;p.vatBp=2000;
 p.lines={{1,10000,true,"materials","1"},{3,2000,true,"",""},{2,12000,false,"Supplier",""}};
 assert(Save(db,p,error));assert(p.entryId>0);assert(count(db,"SELECT COUNT(*) FROM journal_entries")==1);
 assert(count(db,"SELECT COUNT(*) FROM journal_lines")==3);
 Document loaded;assert(Load(db,p.entryId,loaded,error));assert(loaded.vat==2000&&loaded.lines.size()==3);
 assert(loaded.lines[0].analytical=="materials");
 // Edits replace lines and metadata, preserving the same journal entry.
 p.explanation="Edited purchase";p.lines[0].analytical="new materials";
 assert(Save(db,p,error));assert(count(db,"SELECT COUNT(*) FROM journal_entries")==1);
 assert(count(db,"SELECT COUNT(*) FROM am_document_audit")==1);
 assert(Load(db,p.entryId,loaded,error));assert(loaded.explanation=="Edited purchase");
 // Duplicate supplier/invoice/year: atomic rollback, no orphan journal entry.
 Document duplicate=p;duplicate.entryId=0;assert(!Save(db,duplicate,error));
 assert(count(db,"SELECT COUNT(*) FROM journal_entries")==1);
 // Invalid documents never reach the journal.
 Document bad=p;bad.entryId=0;bad.number="BUY-002";bad.vat=1900;
 assert(!Save(db,bad,error));assert(count(db,"SELECT COUNT(*) FROM journal_entries")==1);
 bad=p;bad.entryId=0;bad.number="BUY-003";bad.lines={{7,10000,true,"",""},{7,10000,false,"",""}};
 assert(!Save(db,bad,error));
 // Editing from old generic journal is blocked for new structured documents.
 assert(!sql(db,"UPDATE journal_entries SET description='Bypass' WHERE id=1"));
 assert(!sql(db,"DELETE FROM journal_lines WHERE entry_id=1"));
 // Sales invoice uses a separate customer and account mapping.
 Document sale;sale.kind=2;sale.partnerId=2;sale.number="SALE-001";sale.date=p.date;sale.registerDate=p.date;
 sale.explanation="Service";sale.taxBase=30000;sale.vat=6000;sale.vatBp=2000;
 sale.lines={{4,36000,true,"",""},{5,30000,false,"",""},{6,6000,false,"",""}};
 assert(Save(db,sale,error));
 assert(count(db,"SELECT COUNT(*) FROM journal_entries")==2);
 assert(count(db,"SELECT SUM(l.debit) FROM journal_lines l JOIN journal_entries e ON e.id=l.entry_id") == 48000);
 assert(count(db,"SELECT SUM(l.credit) FROM journal_lines l JOIN journal_entries e ON e.id=l.entry_id") == 48000);
 // Gross-derived VAT rounding can differ by one cent from base-derived VAT.
 Document gross=p;gross.entryId=0;gross.number="BUY-GROSS";gross.taxBase=2;gross.vat=1;
 gross.vatMethod=1;gross.lines={{1,2,true,"",""},{3,1,true,"",""},{2,3,false,"",""}};
 assert(Save(db,gross,error));
 Document grossLoaded;assert(Load(db,gross.entryId,grossLoaded,error));
 assert(grossLoaded.vatMethod==1 && grossLoaded.vat==1);
 Document wrongMethod=gross;wrongMethod.entryId=0;wrongMethod.number="BUY-METHOD";wrongMethod.vatMethod=0;
 assert(!Save(db,wrongMethod,error));
 // Old entries remain visible and are not misidentified as workspace documents.
 assert(sql(db,"INSERT INTO journal_entries(date,document,description) VALUES('2025-02-01','LEGACY','Old');"));
 assert(!IsWorkspaceDocument(db,4)); assert(IsWorkspaceDocument(db,p.entryId));
 // Year lock protects both new entries and changes to previously entered data.
 assert(sql(db,"CREATE TABLE am_fiscal_year(year INTEGER PRIMARY KEY,locked INTEGER); INSERT INTO am_fiscal_year VALUES(2026,1);"));
 Document another=p;another.entryId=0;another.number="BUY-004";
 assert(!Save(db,another,error));assert(!Save(db,p,error));
 assert(count(db,"SELECT COUNT(*) FROM journal_entries")==4);
 sqlite3_close(db);
 assert(ValidDate("2024-02-29")&&!ValidDate("2025-02-29")&&!ValidDate("2026-99-99"));
 std::cout<<"Documents workflow database tests: PASS\n";
}
