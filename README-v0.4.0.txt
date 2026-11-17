AME v0.4.0 — Professional workflow workpapers
=============================================
TARGET: Windows 10, Win32/SQLite, based on the actual v0.3.0-buildfix project.

WHAT ACTUALLY WORKS IN THIS UPDATE
----------------------------------
1. VAT workpaper: manually enter and edit purchase/sales documents with
   tax period, document type, number, date, counterparty, VAT number,
   taxable base, VAT amount, currency and internal treatment code. List
   documents; create period workpaper; show legacy AME invoices separately
   to avoid blending unknown historical currencies; prevent a matching
   old-module invoice from being entered twice. Export a clearly marked
   UTF-8 review TXT file via Save As.
2. Payroll: choose an employee already entered in the personnel directory;
   enter gross earnings, employee and employer insurance, tax and other
   deductions per month; calculate net pay and total employer expense;
   persist, edit, view the monthly register and export a workpaper.
3. Employment: choose an employee and prepare persistent drafts of
   employment contracts, addenda, termination/leave orders and internal
   employment-record drafts; show the record details and export a readable
   non-statutory text draft.
4. Fixed assets: monthly accounting and manually entered tax depreciation
   register, proposal of a straight-line monthly accounting amount from
   original cost and life, and protection against depreciation over the
   asset cost. View totals and export a workpaper.
5. Year-end: journal debit/credit control totals; year-specific counts of
   VAT/payroll/employment/depreciation workpapers; save and export a dated
   year-end review record. No close posting is silently created.
6. Internal change-log triggers for inserts/edits of the new records.
7. The existing company manager, chart of accounts, journal, invoices,
   payments, period picker, reports, and other code were not removed.

NOT IMPLEMENTED / NOT SAFE FOR REAL CLIENT FILINGS
--------------------------------------------------
* NO official NAP DEKLAR.TXT, POKUPKI.TXT, PRODAGBI.TXT, VIES.TXT.
  The TXT export here is a UTF-8 INTERNAL REVIEW and deliberately does not
  claim NAP compatibility. Official Appendix 12 is fixed-width cp1251 and
  has mandatory detailed columns, codes, calculations, and 2026 changes.
* NO automated legally correct social-insurance/tax calculations.
  Rates, thresholds, reliefs, insured-person categories, exemptions,
  sickness, periods, etc. require complete normative rules and validation.
  Entered amounts are workpapers only; NO D1/D6 submission files.
* NO approved employment-contract templates and NO official EETZ XML
  submission or verified accepted-submission register. Text is a DRAFT.
* NO statutory tax depreciation by asset category and ZKPO rules,
  accounting posting, or final year-end close/locking/transfer entries.
* Existing VAT workpaper records do NOT automatically create journal
  postings. The old invoice module still handles its own postings.
* All outputs require accountant and legal/technical verification before
  any production use. Use fictional data to test.

BUILD
-----
1. First back up %LOCALAPPDATA%\AME and commit the working source to Git.
2. Replace source files and CMakeLists.txt in the EXISTING project.
   Keep third_party\sqlite, your .git directory and data folders.
3. In the existing Developer Command Prompt:

   cd /d C:\Users\todor\Documents\AME
   cmake -S . -B build-v040 -A Win32
   cmake --build build-v040 --config Release
   build-v040\Release\AME.exe

TEST (Windows 11)
-----------------
A. Select company; create a fictional supplier purchase in
   Обща отчетност > Дневник на покупките. Save, select, edit, review,
   and export a TXT to the Desktop. The saved row should survive restart.
B. Create an employee in Заплати > Библиотека на персонала. Open
   Заплати > Въвеждане на начисления и удръжки, enter period 2026-11,
   gross 1000.00, employee insurance 130.00, employer 180.00,
   tax 87.00, other 0.00; expected net 783.00, cost 1180.00.
C. Open Назначения > Трудов договор, select that employee, enter
   fictional dates, a document number and basis. Save and export draft.
D. Create a 1200.00 / 12 month fictional fixed asset in
   Амортизации > Инвентарна книга; then open Амортизации >
   Начисляване на амортизациите, choose the asset and month,
   click Предложи месечна; expected 100.00 as a suggested amount.
E. Open Администратор > Годишно приключване: it displays a diagnostic
   and can record a review, NOT a final year-end close.
F. Switch companies and verify none of the new entries leaks across.
G. Confirm existing Сметкоплан, journal and invoices still open.

COMPATIBILITY / IMPLEMENTATION NOTES
------------------------------------
* New data is kept in each company's own existing SQLite file.
* Existing tables and account IDs are not overwritten or replaced.
* No deletion buttons in new regulated workpapers; changes are logged.
* C++14 Win32; new source compiles in a C++ syntax check with Win32 API
  stubs, and its SQLite schema passed a separate insert/duplicate test.
  This is NOT a full MSVC build/run verification; report any compiler errors.
* References to verify against: official NAP VAT page, the current
  Appendix 12 to PPZDDS, NAP 2026 software version 15.01, NAP EETZ/XSD,
  and current N-13 / D1 / D6 documentation.
