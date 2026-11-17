AME v0.2.1 - navigation rebuild (NOT complete professional accounting software)

CHANGED: six main shortcuts (Firma, Dokumenti, Opis, Spravki, Period, Zaplati); main dashboard layout; top-level menu sections; previous invoicing module still accessible as legacy module.
WORKING EXISTING FEATURES: company selection, document journal, document register, reports, chart of accounts, opening balances, legacy invoice module.
NOT IMPLEMENTED: period management, payroll, employment, depreciation, statutory NAP exports and full selected administrator functions. Disabled or clearly marked in development.

INSTALL: Back up %LOCALAPPDATA%\AME and commit current Git state. Extract ZIP into existing AME folder without deleting third_party, build, .git.
BUILD: cmake -S . -B build -A Win32 && cmake --build build --config Release
RUN: build\Release\AME.exe

No Windows compiler was available here; build and runtime remain unverified.
