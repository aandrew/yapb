// SPDX-License-Identifier: Unlicense
#include "tstrategy.h"
#include <cassert>
#include <cstdio>
using namespace bot;
int main () {
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
