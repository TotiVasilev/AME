#include "ame_calculations.h"
#include <algorithm>
#include <limits>

namespace ame {
const wchar_t* const kFundNames[kFundCount]={
 L"ДОО — Пенсии",L"ДЗПО — УПФ",L"ДОО — ОЗМ",
 L"ДОО — Безработица",L"Здравно осигуряване",L"ТЗПБ"
};
static constexpr Money kMaxAmount=9000000000000LL;
bool Percent(Money value,int bp,Money& result){
 if(bp<0||bp>10000||value>kMaxAmount||value< -kMaxAmount)return false;
 // kMaxAmount * 10000 is safely below signed 64-bit max.
 const Money numerator=value*bp;
 result=numerator>=0?(numerator+5000)/10000:-((-numerator+5000)/10000);
 return true;
}
bool VatFromBase(Money base,int vatBp,Money& tax,Money& gross){
 if(vatBp!=0&&vatBp!=900&&vatBp!=2000)return false;
 if(!Percent(base,vatBp,tax))return false;
 if(base>kMaxAmount-tax||base< -kMaxAmount-tax)return false;
 gross=base+tax;return true;
}
bool VatFromGross(Money gross,int vatBp,Money& net,Money& tax){
 if(vatBp!=0&&vatBp!=900&&vatBp!=2000)return false;
 if(gross>kMaxAmount||gross< -kMaxAmount)return false;
 // Half-up rounded tax portion of total, with exact reconciliation.
 Money num=gross*vatBp;int divisor=10000+vatBp;
 tax=num>=0?(num+divisor/2)/divisor:-((-num+divisor/2)/divisor);
 net=gross-tax;return true;
}
bool Payroll(const PayrollInput& in,PayrollResult& r,std::wstring& error){
 r={};error.clear();
 if(in.gross<0||in.gross>kMaxAmount||in.insuranceBase<0||in.insuranceBase>kMaxAmount||
    in.taxRelief<0||in.taxRelief>kMaxAmount||in.otherDeductions<0||
    in.otherDeductions>in.gross||in.incomeTaxBp<0||in.incomeTaxBp>10000){
   error=L"Невалидни суми, осигурителна основа или данъчна ставка.";return false;
 }
 if(in.insuranceBase>in.gross){
   error=L"Осигурителната основа не може автоматично да надвишава брутото; проверете законовите изключения.";return false;
 }
 for(int i=0;i<kFundCount;++i){
  if(!Percent(in.insuranceBase,in.funds[i].employeeBp,r.employeeByFund[i])||
     !Percent(in.insuranceBase,in.funds[i].employerBp,r.employerByFund[i])){
   error=L"Невалидна ставка в осигурителен фонд.";return false;
  }
  r.employeeInsurance+=r.employeeByFund[i];
  r.employerInsurance+=r.employerByFund[i];
 }
 if(r.employeeInsurance+in.otherDeductions>in.gross){
  error=L"Личните осигуровки и другите удръжки надвишават брутото.";return false;
 }
 r.taxableIncome=std::max<Money>(0,in.gross-r.employeeInsurance-in.taxRelief);
 if(!Percent(r.taxableIncome,in.incomeTaxBp,r.incomeTax))return false;
 if(r.employeeInsurance+r.incomeTax+in.otherDeductions>in.gross){
  error=L"Нетното възнаграждение би станало отрицателно.";return false;
 }
 r.net=in.gross-r.employeeInsurance-r.incomeTax-in.otherDeductions;
 r.employerCost=in.gross+r.employerInsurance;
 return true;
}
static bool digits(const std::wstring& s){return !s.empty()&&s.find_first_not_of(L"0123456789")==std::wstring::npos;}
bool ParseMoney(const std::wstring& raw,Money& result,bool allowNegative){
 result=0;size_t l=raw.find_first_not_of(L" \t\r\n"),r=raw.find_last_not_of(L" \t\r\n");
 if(l==std::wstring::npos)return false;
 std::wstring s=raw.substr(l,r-l+1);
 bool neg=s[0]==L'-';if(neg){if(!allowNegative)return false;s.erase(0,1);}
 size_t dot=s.find_first_of(L".,");std::wstring a=s.substr(0,dot),b=dot==std::wstring::npos?L"":s.substr(dot+1);
 if(!digits(a)||a.size()>10||b.size()>2||(!b.empty()&&!digits(b)))return false;
 Money v=0;for(wchar_t c:a)v=v*10+c-L'0';v*=100;
 if(b.size()==1)v+=(b[0]-L'0')*10;
 else if(b.size()==2)v+=(b[0]-L'0')*10+b[1]-L'0';
 if(v>kMaxAmount)return false;
 result=neg?-v:v;return true;
}
bool ParsePercentBp(const std::wstring& s,int& bp){
 Money asCents=0;if(!ParseMoney(s,asCents)||asCents>10000)return false;
 bp=static_cast<int>(asCents);return true;
}
std::wstring FormatMoney(Money v){
 bool neg=v<0;if(neg)v=-v;std::wstring cents=std::to_wstring(v%100);
 return (neg?L"-":L"")+std::to_wstring(v/100)+L"."+(cents.size()==1?L"0":L"")+cents;
}
}
