AME v0.5.0 -- accounting safeguards and regulatory WORKBENCHES
================================================================

IMPORTANT: THIS IS DEVELOPMENT SOFTWARE, NOT SOFTWARE APPROVED FOR FILING.
No feature in v0.5.0 generates official NRA/НАП submission files. Real
clients must NOT use the resulting drafts for tax, payroll or employment
filings. A Bulgarian accountant/payroll professional must validate all rules.

ORIGIN
------
Based directly on AME v0.4.0 Professional Workspaces source archive. All
original source files are included. The original workflows, company database
paths, 144-account chart, company management, journal and VIVA-inspired
menus are preserved. New screens add functionality to selected menu items.
No production databases or SQLite amalgamation files are shipped here.

HOW TO INSTALL
--------------
1. Close ALL running AME.exe instances.
2. Back up %LOCALAPPDATA%\AME (including companies/*.db) to another folder.
3. Commit your current working source in Git before changing it.
4. Extract this archive into C:\Users\todor\Documents\AME and replace the
   existing source files. DO NOT delete .git, third_party, or data folders.
5. In Developer Command Prompt:

   cd /d C:\Users\todor\Documents\AME
   cmake -S . -B build-v050 -A Win32
   cmake --build build-v050 --config Release
   build-v050\Release\AME.exe

New workbenches are reachable from the main menu:
  Обща отчетност > Декларация по ДДС / Запис на файлове за ДДС:
      Monthly tax-data review and validation, not official submission.
  Заплати > Въвеждане на начисления и удръжки / Настройка на заплатите:
      Rate-profile calculator and write-back to the existing payroll table.
  Заплати > Запис на Декларация 1 / Запис на Декларация 6:
      D1/D6 data-reconciliation view, NOT official files.
  Назначения > Запис на XML файл трудов запис:
      Internal XML draft using a namespace explicitly marked AME internal,
      NOT the NRA ETZ XML schema.
  Амортизации > Начисляване на амортизациите / Амортизационен план данъчен:
      Asset-specific tax category/rate/base, annual/monthly schedule and
      persistence into am_depr_work.tax; existing accounting figures kept.
  Администратор > Годишно приключване:
      Actual double-entry journal closing from revenue and expense accounts
      to 123 and a persistent fiscal-year lock, in ONE SQLite transaction.

NEW BEHAVIOR
------------
- Financial period lock is enforced through SQLite triggers on journal
  entries/lines, opening balances, invoices/payments, VAT workpapers,
  payroll workpapers/rate profiles, employment drafts and depreciation.
- Locked-year changes attempted from OLD AME windows will also be rejected.
- Year end closing refuses any unbalanced/incomplete journal entry.
- Closing refuses a missing active account 123, an empty year, or an already
  closed year. It creates a journal document named AME-CLOSE-YYYY.
- Closing is intentionally NOT reversible through the UI. Use a backed-up
  test company. A real reopening workflow requires an accountant-approved
  correction policy and audit trail and is not yet implemented.
- Tax depreciation is capped at the configured basis, uses approved
  categories/rates, does not erase previous accounting depreciation, and
  refuses to overwrite manually entered nonzero tax depreciation.
- Payroll percentages are USER SUPPLIED. The application does NOT supply
  universal statutory insurance rates or legal minimum/maximum thresholds.
- A confirmation is required before writing back calculated amounts.
- The review/export button writes UTF-8 INTERNAL draft files after explicit
  confirmation. Do not upload these to the NRA portal.

TEST ON FICTIONAL DATA
----------------------
1. Create a disposable AME test company and back up its database.
2. Enter two balanced journal transactions using revenue (e.g. 703) and
   expense (e.g. 602), select the company, open year-end preview.
3. Check debit/credit totals. Click close only for a YEAR YOU WILL NOT
   NEED TO EDIT again. Verify the AME-CLOSE-YYYY journal entry is balanced.
4. Try creating another journal entry in that year: SQLite should reject it.
   Creating one in a subsequent year should still work.
5. Add employee records, manually create gross payroll workpaper entries,
   enter explicitly confirmed rate percentages/base limits and review net
   amounts. Apply to TEST payroll and compare stored values. Open D1/D6
   workpaper and check totals. DO NOT use rates as statutory by default.
6. Create sample employment drafts, open internal XML preview and save it.
7. Create a fictional asset; select category, tax basis, annual rate and
   first amortization month, then generate a year's tax schedule. Check the
   amount and verify that accounting-depreciation cells are unchanged.
8. Enter a few monthly VAT workpapers, check the internal validation view.

LEGAL / FEATURE BOUNDARIES
--------------------------
VAT: official DEKLAR.TXT, POKUPKI.TXT, PRODAGBI.TXT must match the then-
current Appendix No.12 of PPZDDS and encode CP1251 fixed-width data.
AME currently lacks required statutory VAT columns/codes and does not
produce those three official files.

Payroll: monthly rate calculator is NOT a full payroll implementation:
no employee-category branching, paid/unpaid leave, sickness, minimum
insurance income by activity/job, exceptions, pension status, contributions
by fund, tax relief, yearly reconciliations, and other special cases.

D1/D6: missing statutory codes/days/funds and 2026 electronic record format.
The built-in D1/D6 screen generates an internal reconciliation only.

Employment ETZ: the internal XML does NOT match the NRA XSD and must never
be submitted as an official employment record. The XSD and code lists are
versioned by the NRA, including 2026 changes.

Depreciation: accountant selects tax basis, category, start month, annual
rate. The schedule is not a substitute for compliant tax/accounting asset
registers (improvements, retirements, exceptions, first/next month choice,
nondepreciable assets, etc.). Category II 50% exception is not automated.

Year-end: closing to 123 and locking ARE implemented, but this DOES NOT
execute comprehensive Bulgarian statutory year-end procedures (corporate
tax, deferred taxes, adjustments, inventory, tax balance sheet, annual
financial statements). Only use after accountant-led review.

OFFICIAL REFERENCES (review for changes before any filing)
-----------------------------------------------------------
PPZDDS App 12: https://www.minfin.bg/upload/54376/PRAVILNIK_za_prilagane_na_Zakona_za_danak_varhu_dobavenata_stojnost.pdf
NRA VAT: https://nra.bg/wps/wcm/connect/agency/site/taxes/dds-v-balgariya/dds-bulgaria
NRA payroll: https://nra.bg/wps/wcm/connect/agency/site/uslugi/deklarirane-na-osiguritelni-vnoski-ob1-ob6
NRA ETZ: https://nra.bg/wps/wcm/connect/nra.bg25863/agency/site/osiguryavane/Registur_na_zaetostta/Registur_na_zaetostta
CITA/ЗКПО: see art. 55 and 58 and amendments effective for tax year.

TEST COVERAGE
-------------
The new compliance_workbench.cpp passed Clang C++14 syntax checking against
Win32 API shims in the build environment. An independent Python SQLite
integration test passed the lock-trigger SQL checks. No MSVC/Windows runtime
build is possible here; YOUR Windows compiler and real accounting tests are
still required. These tests DO NOT verify legal compliance.

PREVIOUS FEATURE LIST DOES NOT TURN INTO 'FULLY COMPLETED' AT v0.5.0.
This is a real increment to all six workflows, not a claim that all six
statutory responsibilities are finished.
