AME 0.2.2 — BUILT-IN BULGARIAN CHART OF ACCOUNTS
=================================================

CHANGES
- 144 editable default accounts preloaded for each new company.
- Existing companies automatically get missing standard accounts ONCE, the next
  time their accounting database is opened by AME.
- Existing code, names, kinds, IDs, journal entries and opening balances are preserved.
- Сметкоплан now includes live search (number or name), section filter (1-9),
  account count and a button to add missing template accounts on demand.
- Custom accounts and edits remain available. Inactive accounts remain inactive.
- No changes to document entry, menus, register, journal, reports or other modules.

IMPORTANT ACCOUNTING NOTE
- This is a comprehensive, editable STARTING chart, not the exact VIVA chart,
  not a mandatory national chart, and not the НАП SAF-T reference chart.
- Actual classification and accounts need accountant review before real data.
- Existing database schema only supports asset/liability/equity/income/expense.
  Mixed accounts, analytics and off-balance tracking still require design.

INSTALL
1. Close AME and commit your working project in VS Code.
2. Back up %LOCALAPPDATA%\AME (actual company databases).
3. Extract this ZIP over C:\Users\todor\Documents\AME.
   Keep .git, third_party, build and any real data unchanged.
4. In Developer Command Prompt:
     cd /d C:\Users\todor\Documents\AME
     cmake -S . -B build-chart -A Win32
     cmake --build build-chart --config Release
     build-chart\Release\AME.exe
   This new folder avoids your previously corrupted build/sqlite3.lib.

TEST ON WINDOWS 11
A. Create a fictional new company; open Сметкоплан, verify 144 preloaded accounts.
B. Search "4531", then by a Bulgarian name, and filter section 4.
C. Edit name of existing account; restart and confirm it persists.
D. Add a custom account (e.g. 999.1), and confirm it stays after restarting.
E. Open an existing test company with 10 example accounts and transactions:
   standard missing accounts appear, but pre-existing codes/names/balances remain unchanged.
F. Press "Добави липсващите стандартни сметки" twice; second pass reports 0 added.
G. Journal, opening balances, Описи, companies and other earlier functions still open.

STATUS
- Source ZIP inspected and verified; NOT Windows-compiled in this environment.
- NOT approved for tax filing or commercial accounting use.
