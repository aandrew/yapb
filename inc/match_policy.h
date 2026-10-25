// SPDX-License-Identifier: Unlicense
#pragma once
namespace bot {
enum class MatchAction { None, Halftime, Overtime, OvertimeHalf, NextOvertime, Finished };
struct MatchRules {
  int a {}, b {}, overtime_base {}, overtime_rounds {};
  bool swapped {}, overtime {}, finished {};
  void Reset () { const bool sides = swapped; *this = {}; swapped = sides; }
  MatchAction Win (bool team_a) {
    if (finished) { return MatchAction::None; }
    if (team_a) { ++a; } else { ++b; }
    if (!overtime) {
      if (a == 12 && b == 12) { overtime = true; overtime_base = 12; overtime_rounds = 0; return MatchAction::Overtime; }
      if (a >= 13 || b >= 13) { finished = true; return MatchAction::Finished; }
      if (a + b == 12) { swapped = !swapped; return MatchAction::Halftime; }
    }
    else {
      ++overtime_rounds;
      if (a - overtime_base >= 4 || b - overtime_base >= 4) { finished = true; return MatchAction::Finished; }
      if (overtime_rounds == 3) { swapped = !swapped; return MatchAction::OvertimeHalf; }
      if (overtime_rounds == 6) { overtime_base += 3; overtime_rounds = 0; return MatchAction::NextOvertime; }
    }
    return MatchAction::None;
  }
  bool MustBuy () const {
    if (finished) { return false; }
    return overtime ? overtime_rounds % 3 == 2 || a - overtime_base == 3 || b - overtime_base == 3 :
      ((a + b) % 12 == 11 || a == 12 || b == 12);
  }
  int SideScore (bool ct) const { return ct != swapped ? a : b; }
};
}
