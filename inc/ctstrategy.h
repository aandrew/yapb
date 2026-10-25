// SPDX-License-Identifier: Unlicense
#pragma once

namespace bot {
enum class CTPhase { Opening = 0, Gather, Cautious, Commit, Retake, LocateBomb, TCall, TClear, TExecute, TPostPlant };
struct CTEvidence {
  bool deployed {}, planted {}, bomb_recent {};
  char planted_site { '-' }, bomb_site { '-' }, previous_target { '-' };
  int sightings_a {}, sightings_b {}, deaths_a {}, deaths_b {};
};
struct CTDecision {
  CTPhase phase { CTPhase::Opening };
  char site { '-' };
};
inline const char *CTPhaseName (CTPhase phase) {
  switch (phase) {
  case CTPhase::Opening: return "round_start";
  case CTPhase::Gather: return "gather_info";
  case CTPhase::Cautious: return "cautious_rotate";
  case CTPhase::Commit: return "committed_rotate";
  case CTPhase::Retake: return "retake";
  case CTPhase::LocateBomb: return "locate_bomb";
  case CTPhase::TCall: return "t_call";
  case CTPhase::TClear: return "t_clear";
  case CTPhase::TExecute: return "t_execute";
  case CTPhase::TPostPlant: return "t_postplant";
  }
  return "invalid";
}
inline CTDecision EvaluateCTState (const CTEvidence &e) {
  if (e.planted) {
    return { e.planted_site == 'A' || e.planted_site == 'B' ? CTPhase::Retake : CTPhase::LocateBomb, e.planted_site };
  }
  const bool a = e.sightings_a >= 2 || (e.sightings_a >= 1 && e.deaths_a >= 1);
  const bool b = e.sightings_b >= 2 || (e.sightings_b >= 1 && e.deaths_b >= 1);
  if (e.bomb_recent && ((e.bomb_site == 'A' && a) || (e.bomb_site == 'B' && b))) {
    return { CTPhase::Commit, e.bomb_site };
  }
  if (a || b) {
    char site = a ? 'A' : 'B';
    if (a && b) {
      site = e.sightings_b > e.sightings_a ? 'B' : 'A';
      if (e.sightings_a == e.sightings_b && (e.previous_target == 'A' || e.previous_target == 'B')) {
        site = e.previous_target;
      }
    }
    return { CTPhase::Cautious, site };
  }
  return { e.deployed ? CTPhase::Gather : CTPhase::Opening, '-' };
}
inline bool ValidateCTPlacement (CTPhase phase, char site, int formation, const char *zones, bool holding) {
  int a = 0, b = 0, mid = 0, short_a = 0, long_a = 0;
  for (int i = 0; i < 5; ++i) {
    switch (zones[i]) {
    case 'A': ++a; break;
    case 'B': ++b; break;
    case 'M': ++mid; break;
    case 'S': ++short_a; break;
    case 'L': ++long_a; break;
    default: return false;
    }
  }
  const bool first = b == 1 && mid == 1 && short_a == 2 && long_a == 1;
  const bool second = b == 1 && mid == 1 && a == 1 && long_a == 2;
  if (phase == CTPhase::Opening || phase == CTPhase::Gather || phase == CTPhase::LocateBomb) {
    return ((phase != CTPhase::Opening) && formation == 0 && holding) ||
      (formation == 1 && first) || (formation == 2 && second);
  }
  if (phase == CTPhase::Cautious) {
    return (site == 'A' && a == 3 && b == 1 && mid == 1) || (site == 'B' && b == 3 && a == 1 && mid == 1);
  }
  if (phase == CTPhase::Commit || phase == CTPhase::Retake) {
    return (site == 'A' && a == 5) || (site == 'B' && b == 5);
  }
  return false;
}
}
