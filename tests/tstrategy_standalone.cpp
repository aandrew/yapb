// SPDX-License-Identifier: Unlicense
#include "tstrategy.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace bot;
int main () {
 bool eligible[5] {false,true,true,true,true};float d[25] {},r[5] {0,1000000,900000,1100000,0};
 const float x[5] {0,0,100,200,4000};for(int i=0;i<5;++i)for(int j=0;j<5;++j)d[i*5+j]=(x[i]-x[j])*(x[i]-x[j]);
 assert(MainGroupAnchor(eligible,d,r,5)!=4);
 for(auto &alive:eligible)alive=false;assert(MainGroupAnchor(eligible,d,r,5)==-1);
 assert(THasManAdvantage(4,5));assert(!THasManAdvantage(4,4));assert(!THasManAdvantage(5,4));
 assert(std::strcmp(TExecuteReason(4,4),"site pick")==0);
 assert(std::strcmp(TExecuteReason(4,5),"man advantage")==0);
 assert(TCanExecuteSitePick(4,true,7,true));assert(!TCanExecuteSitePick(4,false,7,true));
 assert(!TCanExecuteSitePick(0,true,7,true));
 assert(TCanExecuteEarly(4,5,1,7,true));
 assert(!TCanExecuteEarly(4,4,1,7,true));
 assert(!TCanExecuteEarly(4,5,0,7,true));
 assert(!TCanExecuteEarly(4,5,1,7,false));
 assert(!TCanExecuteEarly(4,5,1,9,true));
 assert(TCanRetarget(false,2,3,7,false));
 assert(TCanRetarget(false,2,3,8,false));
 assert(!TCanRetarget(true,2,3,7,false));
 assert(!TCanRetarget(false,1,4,7,false));
 assert(!TCanRetarget(false,3,1,7,false));
 assert(!TCanRetarget(false,2,3,9,true));
 puts("Native casualty-based T execution and retarget guards passed.");
}
