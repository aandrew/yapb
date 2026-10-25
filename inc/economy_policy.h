// SPDX-License-Identifier: Unlicense
#pragma once
namespace bot {
enum class EconomyMode { Save, Half, Full, LastRound };
inline const char *EconomyName (EconomyMode mode) {
  switch (mode) { case EconomyMode::Save: return "save"; case EconomyMode::Half: return "half"; case EconomyMode::Full: return "full"; default: return "last_round"; }
}
inline bool MustBuyMR12 (int ct, int t, bool known) {
  return known && (ct == 12 || t == 12 || ((ct + t) < 24 && (ct + t) % 12 == 11));
}
inline EconomyMode ChooseEconomy (int average, int ct, int t, bool known) {
  if (MustBuyMR12 (ct, t, known)) { return EconomyMode::LastRound; }
  return average >= 4500 ? EconomyMode::Full : (average >= 3500 ? EconomyMode::Half : EconomyMode::Save);
}
inline int EconomySpendCap (EconomyMode mode, int money) {
  const int cap = mode == EconomyMode::Save ? 200 : (mode == EconomyMode::Half ? 2500 : money);
  return money < cap ? money : cap;
}
inline int EconomyWeaponBudget (EconomyMode mode, int money, bool armored, bool ct) {
  const int cap = EconomySpendCap (mode, money);
  if (mode == EconomyMode::Save) { return 0; }
  const int reserve = (armored ? 0 : (mode == EconomyMode::Half || money < 3500 ? 650 : 1000)) +
    (mode == EconomyMode::Half ? 150 : 200) + (ct && mode != EconomyMode::Half && money >= 3500 ? 200 : 0);
  return cap > reserve ? cap - reserve : 0;
}
inline bool CanFundDrop (int cash_after_own_buy, int replacement_cost, bool receiver_has_primary, int receiver_cash) {
  return cash_after_own_buy > 4500 && replacement_cost > 0 && cash_after_own_buy >= replacement_cost && !receiver_has_primary && receiver_cash >= 0 && receiver_cash < 3500;
}
}
