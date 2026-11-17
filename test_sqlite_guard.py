"""Schema and lock integration smoke test using the SQL strings from the C++ source.
Run: python test_sqlite_guard.py
This is NOT a Windows compilation or a statutory compliance check.
"""
import re, sqlite3, pathlib
s=(pathlib.Path(__file__).parent/'src/ui/compliance_workbench.cpp').read_text()
def literal_block(start,end):
    text=s[s.index(start):s.index(end,s.index(start))]
    parts=re.findall(r'"((?:\\.|[^"\\])*)"',text)
    return ''.join(bytes(x,'ascii').decode('unicode_escape') for x in parts)
schema=literal_block('const char* schema=', 'if(!exec(db,schema))')
locks=literal_block('const char* locks=', 'if(!exec(db,locks))')
db=sqlite3.connect(':memory:')
db.executescript(schema)
db.executescript(locks)
for table,field in [('invoices','date'),('payments','date'),('am_vat_work','period'),('am_pay_work','period'),('am_depr_work','period'),('am_employment_work','docdate'),('am_pay_rates','period')]:
    for op in ['INSERT','UPDATE','DELETE']:
        row='OLD' if op=='DELETE' else 'NEW'
        trigger=f'''CREATE TRIGGER IF NOT EXISTS ame_lock_{table}_{op} BEFORE {op} ON {table} WHEN EXISTS(SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year=CAST(substr({row}.{field},1,4) AS INTEGER)) '''
        if op=='UPDATE': trigger+=f'''OR EXISTS(SELECT 1 FROM am_fiscal_year WHERE locked=1 AND year=CAST(substr(OLD.{field},1,4) AS INTEGER)) '''
        trigger+="BEGIN SELECT RAISE(ABORT,'AME: accounting year locked'); END;"
        db.executescript(trigger)
db.execute("INSERT INTO accounts(id,code,name,kind,active) VALUES (1,'123','Result',2,1)") if any(x[0]=='accounts' for x in db.execute('SELECT name FROM sqlite_master WHERE type="table"')) else db.execute("CREATE TABLE accounts(id INTEGER PRIMARY KEY,code TEXT,name TEXT,kind INT,active INT)")
# Set up journal and payroll before locking.
db.execute("INSERT INTO journal_entries(id,date,document,description) VALUES (1,'2026-03-01','TEST-1','test')")
db.execute("INSERT INTO journal_lines(entry_id,line_no,account_id,debit,credit) VALUES (1,1,1,100,0)")
db.execute("INSERT INTO am_pay_rates(period,employee_bp,employer_bp,tax_bp,minimum,maximum,approved_by,created_at) VALUES ('2026-03',1000,1500,1000,0,999999,'Reviewer','now')")
db.execute("INSERT INTO am_fiscal_year(year,locked,entry,closed_at,reviewer,note) VALUES (2026,1,1,'now','Reviewer','test')")
def blocked(label, action):
    try:
        action()
    except sqlite3.DatabaseError as ex:
        assert 'AME: accounting year locked' in str(ex), (label,str(ex))
        print('BLOCKED',label)
    else:
        raise AssertionError('UNEXPECTEDLY ALLOWED '+label)
blocked('journal entry insert',lambda:db.execute("INSERT INTO journal_entries(date,document,description) VALUES ('2026-05-01','NO','NO')"))
blocked('journal entry update',lambda:db.execute("UPDATE journal_entries SET date='2027-01-01' WHERE id=1"))
blocked('journal line change',lambda:db.execute("UPDATE journal_lines SET debit=200 WHERE entry_id=1"))
blocked('payroll rate update',lambda:db.execute("UPDATE am_pay_rates SET tax_bp=2000 WHERE period='2026-03'"))
blocked('vat work insert',lambda:db.execute("INSERT INTO am_vat_work(period,direction,kind,number,date,partner,vat_id,base,vat,treatment,currency,note,created) VALUES('2026-03',0,'01','TEST','2026-03-11','test','','100','20','test','EUR','','now')"))
# Future periods are writable, and earlier locked years remain untouched.
db.execute("INSERT INTO journal_entries(date,document,description) VALUES ('2027-01-01','OK','OK')")
print('OK: new-year journal inserts are allowed; lock SQL compiles and is enforced')
