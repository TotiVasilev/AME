#pragma once
// AME calculation primitives. Amounts are integer EUR cents (or the minor
// unit of the selected currency). Percent rates use basis points: 2000=20%.
// This module does NOT decide VAT eligibility or insurance categories.
#include <cstdint>
#include <array>
#include <string>

namespace ame {
using Money = std::int64_t;
constexpr int kFundCount=6;
extern const wchar_t* const kFundNames[kFundCount];
struct FundRate { int employeeBp=0; int employerBp=0; };
struct PayrollInput {
 Money gross=0;
 Money insuranceBase=0;
 Money taxRelief=0;
 Money otherDeductions=0;
 int incomeTaxBp=1000;
 std::array<FundRate,kFundCount> funds{};
};
struct PayrollResult {
 std::array<Money,kFundCount> employeeByFund{};
 std::array<Money,kFundCount> employerByFund{};
 Money employeeInsurance=0, employerInsurance=0;
 Money taxableIncome=0, incomeTax=0, net=0, employerCost=0;
};
// Integer arithmetic with half-up rounding (away from zero for negatives).
// Caps are intentionally conservative; overflow is reported, never wrapped.
bool Percent(Money value,int bp,Money& result);
bool VatFromBase(Money base,int vatBp,Money& tax,Money& gross);
bool VatFromGross(Money gross,int vatBp,Money& net,Money& tax);
bool Payroll(const PayrollInput& in,PayrollResult& result,std::wstring& error);
bool ParseMoney(const std::wstring& text,Money& result,bool allowNegative=false);
bool ParsePercentBp(const std::wstring& text,int& bp);
std::wstring FormatMoney(Money amount);
}
