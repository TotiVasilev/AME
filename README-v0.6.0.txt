AME v0.6.0 — АВТОМАТИЧНИ ИЗЧИСЛЕНИЯ И КОНТРОЛ (17.11.2026)
========================================================

STATUS: DEVELOPMENT BUILD / НЕ ЗА ПОДАВАНЕ КЪМ НАП.

This ZIP contains the complete AME source tree. It deliberately omits third_party/sqlite,
.git, existing build outputs, and all existing company databases. Do NOT delete those folders.

ACTUALLY IMPLEMENTED
--------------------
1) Automatic VAT calculation, using integer minor units (no floating-point values):
   - Документи: enter quantity and unit price, then click 'Изчисли ДДС'.
   - ДДС workpaper: enter the tax base; click 'Изчисли ДДС (20/9/0)', choose
     standard 20%, qualifying 9%, zero-rated with LEGAL BASIS, or calculate
     the net and VAT backwards from a VAT-INCLUSIVE gross amount at 20%/9%.
   - The amount and selected rate are checked on save. The applied VAT rate,
     legal-basis text and source method are saved with new invoices.
   - The software does NOT decide whether a rate/exemption is legally valid.
   - Old posted invoices stay unchanged; classification for НАП is incomplete.
2) Payroll insurance calculator (Заплати -> Настройка на заплатите):
   - Works per company, employee and month; requires an existing monthly gross-pay row.
   - Enter insurance base, tax-relief amount and per-fund employer/employee rates.
   - Calculates per-fund contributions, income tax, net salary, employer cost.
   - Save a reviewer-confirmed profile; then optionally Apply to the monthly payroll.
   - All six fund amounts, historical calculations and audit activities are saved.
   - Can save effective-dated rates by insured-person category and re-use them
     for employees in later months with explicit reviewer confirmation.
   - No fictional legal default insurance percentages. A qualified payroll
     specialist must provide/review category-, date- and employer-specific rates.
3) Д1/Д6 workpaper now checks the stored category, days, НКПД, КИД,
   reviewer and six fund amounts against monthly payroll totals.
   Missing records are flagged as incomplete; still NOT official filing output.
4) Closed fiscal year triggers protect the new payroll data and rate templates.
5) The prior v0.5.0 company management, chart, accounting journal, periods,
   reports, basic employment/VAT/depreciation workspaces, and year-end closing
   code remain included, without intentional feature removal.

NOT IMPLEMENTED / NOT LEGALLY VALIDATED
-------------------------------------
- NO official DEKLAR.TXT / POKUPKI.TXT / PRODAGBI.TXT generator.
  The official reporting requires the FULL and CURRENT Appendix 12 fields,
  classification, statutory calculations and cp1251 fixed-width records.
  Internal .txt / CSV review exports must NEVER be submitted as official files.
- NO validated official D1/D6 submission files and NO payroll for every
  statutory category, sick leave, parental leave, correction, minimum base,
  maximum base, relief or extraordinary employment case.
- NO XSD-validated ETZ XML. Existing XML is an internal, non-statutory draft.
- NO fully statutory ZKPO depreciation exceptions or automatic tax closing
  procedure. The previously implemented accounting year-end journal entry
  and period lock do NOT substitute for statutory closing and an accountant audit.
- The 2026 VAT and personnel file formats may change; the correct publication
  and specific taxpayer scenario must be verified before filing.
- Windows executable NOT built/tested in this environment; use your MSVC 2026
  build and run the checks on fictional company data.

BACK UP FIRST
-------------
1. Commit AME source in your existing Git repo.
2. Back up the WHOLE %LOCALAPPDATA%\AME folder on Windows.
3. Close AME.
4. Extract this ZIP over the project source folder; keep third_party/sqlite
   and .git. No accounting company data is included.

BUILD (Windows 11 developer machine / Windows 10 target)
------------------------------------------------------
cd /d C:\Users\todor\Documents\AME
cmake -S . -B build-v060 -A Win32
cmake --build build-v060 --config Release
build-v060\Release\AME.exe

TEST ON A FICTIONAL COMPANY
---------------------------
A) ДДС: create the VAT workpaper, enter tax base 100.00 EUR, click
   'Изчисли ДДС', select 20%. Expect VAT 20.00, gross 120.00.
B) Reverse VAT: put 120.00 in the base field, select
   '20% — изчисли ДДС от обща сума с данък'. Expect net 100.00 and VAT 20.00.
C) Reduced rate: 100.00 base / 9% -> VAT 9.00 (only if legally eligible).
D) 0%: AME should require a manually entered legal basis before saving.
E) Create a sample employee (Заплати > Библиотека на персонала), create
   a gross-pay row (e.g. 2000.00) from 'Въвеждане на начисления и удръжки'.
F) Open Заплати > Настройка на заплатите; choose month & employee. Enter
   insured type, reviewer, category-specific employee/employer contributions,
   worked days, NKPD, KID and insurance base. Use Preview, Save rates,
   confirm Apply to monthly payroll. Check the updated monthly record.
G) Test 'Запази ставки за вид осигурен', then reload those rates with
   'Зареди ставки за вид осигурен'. Confirmation is required, and templates
   remain date-specific. Check official rates with a payroll specialist.
H) Open Д1/Д6 review and confirm missing fields/funds are highlighted.
I) Test locked-year protection ONLY ON A THROWAWAY company, with backups.

SOURCE REGRESSION TESTS (requires g++ / Python, not needed for normal build)
----------------------------------------------------------------------------
g++ -std=c++14 src/data/ame_calculations.cpp tests/calculations_test.cpp -o calculations_test
calculations_test
python tests/fund_schema_test.py

LEGAL REFERENCE (VERIFY CURRENT EDITION BEFORE FILING)
-------------------------------------------------------
Bulgarian Ministry of Finance VAT guidance: https://www.minfin.bg/bg/790
NRA published ETZ technical changes: https://nra.bg/wps/wcm/connect/nra.bg25863/agency/site/actualno/Publikuvani_sa_promeni_vuv_failovia_format_za_ETZ

Do not input real payroll data until the accounting and compliance workflows
have been checked by a qualified Bulgarian accountant and payroll specialist.
