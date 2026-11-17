AME v0.2.0 — FIRST PROFESSIONAL WORKFLOW UPDATE (development build)

Based on the uploaded AME v0.1.0 source archive.

WHAT CHANGED
- New read-only "Опис" window listing the active company's accounting documents.
- Filter by start date, end date, and document number/description.
- Document list includes journal operations, invoice postings, and payment postings (when available).
- The former sidebar "Фактури" label is now "Документи". It still opens the EXISTING invoice/partner/payment form; its behavior has not been removed.
- Dashboard quick action "Нова фактура" is now "Опис"; invoice entry remains available via "Документи" in the sidebar and the accounting menu.
- Existing company management, journal, chart of accounts, opening balances, reports, invoices, payments, settings and database schema are preserved.
- No existing data migration or destructive database change.

INSTALL
1. Commit the working v0.1.0 project in Git and back up %LOCALAPPDATA%\AME.
2. Extract this archive into your existing AME project folder, replacing source files.
3. Keep third_party/sqlite, build folders and .git from the existing project.
4. From Developer Command Prompt:
   cd /d C:\Users\todor\Documents\AME
   cmake -S . -B build -A Win32
   cmake --build build --config Release
   build\Release\AME.exe

TEST
1. Select a company containing a previously saved accounting operation.
2. Open Опис from the dashboard or Счетоводство > Опис на документи.
3. Confirm that the operation appears once and shows the correct date, number and debit total.
4. Filter by document number, date and description.
5. Open Документи and verify existing invoice/partner/payment functions still work.
6. Switch companies and verify the Опис contents change.
7. Verify Сметкоплан, Начални салда, Операции and Справки still open.

KNOWN LIMITATIONS
- This is an initial working slice, not the full VIVA-equivalent application.
- Опис is read-only; edit via the existing journal window.
- No statutory НАП export is implemented or certified in this update.
- Payroll, employment, fixed assets and other selected modules remain future work.
- This source has not been compiled on Windows in this environment; local compilation is required.
- Use fictional data. Do not use for production accounting or regulatory submission.
