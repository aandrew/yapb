// SPDX-License-Identifier: Unlicense
#include "economy_policy.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace bot;
int main () {
  assert (ChooseEconomy (2500,0,0,true) == EconomyMode::Save);
  assert (ChooseEconomy (3499,0,0,true) == EconomyMode::Save);
  assert (ChooseEconomy (3500,0,0,true) == EconomyMode::Half);
  assert (ChooseEconomy (4499,0,0,true) == EconomyMode::Half);
  assert (ChooseEconomy (4500,0,0,true) == EconomyMode::Full);
  assert (ChooseEconomy (800,6,5,true) == EconomyMode::LastRound);
  assert (ChooseEconomy (800,11,12,true) == EconomyMode::LastRound);
  assert (ChooseEconomy (800,12,1,true) == EconomyMode::LastRound);
  assert (ChooseEconomy (800,11,0,false) == EconomyMode::Save);
  assert (!MustBuyMR12 (6,4,true));
  assert (!MustBuyMR12 (13,11,true));
  assert (EconomyWeaponBudget (EconomyMode::Full,4500,false,true) == 3100);
  assert (EconomyWeaponBudget (EconomyMode::Full,4500,false,true) < 4750);
  assert (EconomyWeaponBudget (EconomyMode::Half,3500,false,false) == 1700);
  assert (EconomyWeaponBudget (EconomyMode::Save,8000,false,false) == 0);
  assert (EconomySpendCap (EconomyMode::Half,800) == 800);
  assert (EconomySpendCap (EconomyMode::LastRound,800) == 800);
  assert (!CanFundDrop (4500,2500,false,800));
  assert (CanFundDrop (4501,2500,false,800));
  assert (!CanFundDrop (4600,4750,false,800));
  assert (!CanFundDrop (8000,2500,true,800));
  assert (!CanFundDrop (8000,2500,false,-1));
  assert (!CanFundDrop (8000,2500,false,3500));
  for (int cash=0; cash<=16000; cash+=100) {
    for (auto mode : {EconomyMode::Save,EconomyMode::Half,EconomyMode::Full,EconomyMode::LastRound}) {
      assert (EconomySpendCap (mode,cash) <= cash);
      assert (EconomyWeaponBudget (mode,cash,false,true) <= cash);
    }
  }
  puts ("Native economy thresholds, MR12, affordability and drop assertions passed.");
}
