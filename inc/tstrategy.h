// SPDX-License-Identifier: Unlicense
#pragma once
namespace bot {
inline int MainGroupAnchor (const bool *eligible, const float *distances_sq, const float *route_distances_sq, int count) {
  int best = -1, largest = 0;
  for (int i = 0; i < count; ++i) {
    if (!eligible[i]) { continue; }
    int members = 0;
    for (int j = 0; j < count; ++j) { if (eligible[j] && distances_sq[i * count + j] <= 640.0f * 640.0f) { ++members; } }
    if (members > largest || (members == largest && (best < 0 || route_distances_sq[i] < route_distances_sq[best]))) { best = i; largest = members; }
  }
  return best;
}
inline bool THasManAdvantage (int ct_alive, int t_alive) { return t_alive > ct_alive; }
inline const char *TExecuteReason (int ct_alive, int t_alive) { return THasManAdvantage (ct_alive, t_alive) ? "man advantage" : "site pick"; }
inline bool TCanExecuteSitePick (int t_alive, bool confirmed_site_pick, int phase, bool round_live) {
  return round_live && phase == 7 && t_alive > 0 && confirmed_site_pick;
}
inline bool TCanExecuteEarly (int ct_alive, int t_alive, int recent_enemy_deaths, int phase, bool round_live) {
  return round_live && phase == 7 && THasManAdvantage (ct_alive, t_alive) && recent_enemy_deaths >= 1;
}
inline bool TCanRetarget (bool retargeted, int recent_own_deaths, int t_alive, int phase, bool planted) {
  return !planted && !retargeted && (phase == 7 || phase == 8) && recent_own_deaths >= 2 && t_alive >= 2;
}
}
