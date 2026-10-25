// SPDX-License-Identifier: Unlicense
#pragma once

namespace bot {
enum class CTPhase { Opening = 0, Gather, Cautious, Commit, Retake, LocateBomb, TCall, TClear, TExecute, TPostPlant, TRecoverBomb, GuardBomb, Crunch, TReassess };
inline bool CrunchMixupAllowed (int round, int ct_score, int t_score, int last_crunch) {
  const int gap = ct_score < t_score ? 2 : 4;
  return round > 0 && round % gap == 0 && round - last_crunch >= gap;
}
inline void RemainingCTCoverage (const bool *alive, char *zones) {
  int living[5] {}, count = 0;
  for (int i = 0; i < 5; ++i) { if (alive[i]) { living[count++] = i; } }
  const char *coverage = count == 1 ? "M" : (count == 2 ? "BL" : (count == 3 ? "BML" : (count == 4 ? "BMSL" : "BMSSL")));
  char remaining[5] { 'B','M','S','S','L' }; bool assigned[5] {};
  for (int n = 0; n < count; ++n) { zones[living[n]] = coverage[n]; assigned[living[n]] = true; for (auto &zone : remaining) { if (zone == coverage[n]) { zone = '-'; break; } } }
  int cursor = 0;
  for (int i = 0; i < 5; ++i) { if (!assigned[i]) { while (cursor < 5 && remaining[cursor] == '-') { ++cursor; } if (cursor < 5) { zones[i] = remaining[cursor++]; } } }
}
struct CTBombKnowledge {
  char confirmed_site { '-' };
  bool cleared_a {}, cleared_b {};
  char Site () const {
    if (confirmed_site == 'A' || confirmed_site == 'B') { return confirmed_site; }
    if (cleared_a == cleared_b) { return '-'; }
    return cleared_a ? 'B' : 'A';
  }
};
struct CTEvidence {
  bool deployed {}, planted {}, bomb_recent {};
  char planted_site { '-' }, bomb_site { '-' }, previous_target { '-' };
  int sightings_a {}, sightings_b {}, deaths_a {}, deaths_b {}, intruders_a {}, intruders_b {};
  bool secured_drop {}, lost_a {}, lost_b {};
  char dropped_site { 'M' };
  bool crunch_active {};
  char crunch_choke { '-' };
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
  case CTPhase::TRecoverBomb: return "t_recover_bomb";
  case CTPhase::GuardBomb: return "guard_bomb";
  case CTPhase::Crunch: return "crunch";
  case CTPhase::TReassess: return "t_reassess_after_recovery";
  }
  return "invalid";
}
inline CTDecision EvaluateCTState (const CTEvidence &e) {
  if (e.planted) {
    return { e.planted_site == 'A' || e.planted_site == 'B' ? CTPhase::Retake : CTPhase::LocateBomb, e.planted_site };
  }
  if (e.secured_drop) { return { CTPhase::GuardBomb, e.dropped_site }; }
  const bool a = e.sightings_a >= 2 || (e.sightings_a >= 1 && e.deaths_a >= 1);
  const bool b = e.sightings_b >= 2 || (e.sightings_b >= 1 && e.deaths_b >= 1);
  if (e.bomb_recent && ((e.bomb_site == 'A' && a) || (e.bomb_site == 'B' && b))) {
    return { CTPhase::Commit, e.bomb_site };
  }
  if (e.lost_a || e.lost_b) { return { CTPhase::Commit, e.lost_b && !e.lost_a ? 'B' : 'A' }; }
  if (e.intruders_a >= 3 || e.intruders_b >= 3) { return { CTPhase::Commit, e.intruders_b > e.intruders_a ? 'B' : 'A' }; }
  if (e.crunch_active) { return { CTPhase::Crunch, e.crunch_choke }; }
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
  int a = 0, b = 0, mid = 0, short_a = 0, long_a = 0, guard = 0;
  for (int i = 0; i < 5; ++i) {
    switch (zones[i]) {
    case 'A': ++a; break;
    case 'B': ++b; break;
    case 'M': ++mid; break;
    case 'S': ++short_a; break;
    case 'L': ++long_a; break;
    case 'G': ++guard; break;
    default: return false;
    }
  }
  if (phase == CTPhase::GuardBomb) { return guard == 5 && (site == 'A' || site == 'B' || site == 'M'); }
  const bool first = b == 1 && mid == 1 && short_a == 2 && long_a == 1;
  const bool second = b == 1 && mid == 1 && a == 1 && long_a == 2;
  if (phase == CTPhase::Opening || phase == CTPhase::Gather || phase == CTPhase::LocateBomb) {
    return ((phase != CTPhase::Opening) && formation == 0 && holding) ||
      (formation == 1 && first) || (formation == 2 && second) ||
      (formation == 3 && b == 2 && mid == 1 && a == 1 && long_a == 1) ||
      (formation == 4 && b == 2 && a == 2 && mid == 1) ||
      (formation == 5 && b == 1 && mid == 1 && short_a == 2 && a == 1) ||
      (formation == 6 && b == 4 && a == 1) ||
      (phase == CTPhase::Gather && formation == 8 && a == 3 && b == 1 && mid == 1) ||
      (phase == CTPhase::Gather && formation == 9 && b == 3 && a == 1 && mid == 1);
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
