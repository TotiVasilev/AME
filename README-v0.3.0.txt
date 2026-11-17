AME v0.3.0 — EXPANDED WORKING ACCOUNTING WORKSPACE
====================================================
DEVELOPMENT BUILD — NOT A COMPLETE OR CERTIFIED ACCOUNTING PRODUCT

WHAT HAS ACTUALLY BEEN IMPLEMENTED IN THIS RELEASE
- Main dashboard visibly redesigned with the six actions requested by the
  accountant: Firma, Dokumenti, Opis, Spravki, Period, Zaplati.
- VIVA-inspired top menu names, preserving the user's requested inclusions:
  Obsta otchetnost minus the LAST FOUR options; Schetovodna baza FIRST FIVE
  plus Nomenklatura na razhodite; Amortizatsii all names; Naznacheniya all;
  Zaplati all; Administrator 3-5, 7-18, 22 only.
- Entire menus KALKULATSII, RAZPLASHTANIA and standalone FAKTURIRANE excluded.
- Administrator option 22 corrected against IMG_4617: "Dnevnitsi na Excel i
  spravka" (NOT "Izbor na ustroystvo").
- Functional selected accounting period (year/month), stored per company.
  It preselects date ranges in newly added reports; it DOES NOT lock or close
  a period, nor change the dates of existing transactions.
- Functional CRUD for partner register (same partners as legacy invoices),
  nomenclature of expenses, materials, and warehouses.
- Functional personnel records (names, identifier, role, start date, notes).
  This is NOT a payroll engine or a legal employment-document generator.
- Functional positions, earnings, deductions catalogs (just nomenclatures).
- Functional fixed asset inventory data entry, with a simple straight-line
  monthly *working* estimate. NO statutory tax depreciation engine.
- Accounting workpapers calculated from existing journal and account data:
  trial balance, monthly movements, date and account journals, simple cashbook,
  trial general ledger, internal VAT purchase/sale listing, simple income/
  expense and balance estimates, acquisition list, asset list.
- Opening balance carryforward improved in the NEW report window; it takes
  the latest opening balance snapshot and subsequent journal entries.
- CSV export of generated workpapers in UTF-16LE BOM and semicolon format.
- Existing company management, 144-account chart, opening balances, manual
  entries, journal, invoice/payment prototype, register and legacy reports
  are kept. The original reports are under Krai > Spravki — predishen modul.

NOT IMPLEMENTED — DO NOT ASSUME THESE MENUS ARE FUNCTIONAL
- Legislative НАП VAT declarations and prescribed tax file formats.
- Correct Bulgarian payroll, insurance, PIT, Д1/Д6 and personnel XML filing.
- Compliant employment contracts, orders, electronic labor records.
- Statutory annual balance sheet, cash-flow statement and year-end closing.
- Full fiscal and book depreciation, disposals, tax depreciation plan.
- Complex multi-line VAT invoices, document corrections and tax adjustments.
- Analytical account structures, permissions, audit log, accounting locks.
- Several selected menu items open an explicit 'not yet implemented' notice.
  This is intentional: showing fake reports would risk real financial harm.
- Monetary denomination in old records requires business-specific review
  before real use after Bulgaria's euro transition.

The menu itself is NOT a completion checklist. The above unavailable features
are substantial engineering projects requiring tested calculations, genuine
legal forms and accountant verification. Do NOT use this build for real tax
submissions or sensitive personal records.

INSTALL/BUILD ON WINDOWS 11 FOR WINDOWS 10 TARGET
1. Commit existing working v0.2.2 source using VS Code Source Control.
2. Close AME. Back up the entire folder %LOCALAPPDATA%\AME. This folder
   contains your company databases. Keep the backups OFF your test machine.
3. Extract this ZIP into C:\Users\todor\Documents\AME and overwrite src and
   CMakeLists.txt. Keep .git, third_party/sqlite, and old build folders.
4. Use existing Developer Command Prompt:
     cd /d C:\Users\todor\Documents\AME
     cmake -S . -B build-v030 -A Win32
     cmake --build build-v030 --config Release
     build-v030\Release\AME.exe
   The new build folder avoids the corrupt old sqlite3.lib you encountered.

SUGGESTED SMOKE TEST WITH FAKE DATA ONLY
A. Choose a test company; verify the six main shortcut buttons and new menus.
B. Open "Период"; save 2026/01, close and reopen. In Справки the chosen month
   should prefill 2026-01-01 through 2026-01-31.
C. Enter opening balance 1000.00 Dr in 501 and 1000.00 Cr in 101 on 2026-01-01.
D. In Документи, post a Dr 601 / Cr 501 expense for 100.00 on 2026-01-10.
E. In monthly trial balance, 501 closing amount should reflect 900.00 Dr.
F. Change period to 2026/02: the opening 501 balance should carry over 900.00
   (unlike the old prototype's exact-date-only behavior).
G. Add a fake partner through Счетоводна база > Номенклатура на контрагентите,
   close/reopen, and verify it remains. Then add a material and expense code.
H. Add a fake asset and an employee, close/reopen, verify both persist.
I. Use "CSV експорт" with a NEW full path such as
   C:\Users\todor\Desktop\AME-test-report.csv. Existing files are not overwritten.
J. Check that your existing companies, chart of accounts and journal entries
   are still present. No customer or sensitive personnel data for testing.

TESTING STATUS
- ZIP archive can be verified and SQLite queries were smoke-tested with
  representative fictional journal data.
- The full Windows native application cannot be compiled or executed in
  this environment. Your Visual Studio build is the first real compile test.
- This package is a meaningful source-code step toward the required app,
  not the full commercially usable product the user ultimately requested.
