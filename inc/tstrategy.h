// SPDX-License-Identifier: Unlicense
#pragma once
namespace bot {
inline bool TCanExecuteEarly (int ct_alive, int t_alive, int recent_enemy_deaths, int phase, bool round_live) {
  return round_live && phase == 7 && t_alive > ct_alive && recent_enemy_deaths >= 1;
}
inline bool TCanRetarget (bool retargeted, int recent_own_deaths, int t_alive, int phase, bool planted) {
  return !planted && !retargeted && (phase == 7 || phase == 8) && recent_own_deaths >= 2 && t_alive >= 2;
}
}
