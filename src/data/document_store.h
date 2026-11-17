#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct sqlite3;

namespace ame_doc {
struct Line {
    int accountId = 0;
    std::int64_t amount = 0;  // integer minor units, positive
    bool debit = true;
    std::string analytical;   // UTF-8; optional party/subaccount reference
    std::string quantity;     // optional decimal quantity as entered
};
struct Document {
    std::int64_t entryId = 0;
    int kind = 0;             // 0=free, 1=purchase, 2=sale
    int partnerId = 0;
    int vatBp = 0;            // 0, 900, 2000
    int vatMethod = 0;        // 0=from taxable base; 1=split VAT-inclusive gross
    std::int64_t taxBase = 0;
    std::int64_t vat = 0;
    std::string date;          // YYYY-MM-DD
    std::string registerDate;
    std::string number;
    std::string explanation;
    std::string deal;
    std::string legalBasis;
    std::vector<Line> lines;
};
bool ValidDate(const std::string& date);
bool Balanced(const std::vector<Line>& lines, std::int64_t& debit, std::int64_t& credit,
              std::string& problem);
bool EnsureSchema(sqlite3* db, std::string& error);
bool Save(sqlite3* db, Document& doc, std::string& error);
bool Load(sqlite3* db, std::int64_t entryId, Document& doc, std::string& error);
// True only for entries created in this document workspace.
bool IsWorkspaceDocument(sqlite3* db, std::int64_t entryId);
}  // namespace ame_doc
