// Standalone policy checks use the same header as the game DLL.
#include <assert.h>
#include <stdio.h>
#include "ctstrategy.h"
using namespace bot;
int main () {
  CTEvidence e {};
  assert (EvaluateCTState (e).phase == CTPhase::Opening);
  e.deployed = true; assert (EvaluateCTState (e).phase == CTPhase::Gather);
  e.sightings_b = 1; assert (EvaluateCTState (e).phase == CTPhase::Gather);
  e.sightings_b = 2; assert (EvaluateCTState (e).phase == CTPhase::Cautious && EvaluateCTState (e).site == 'B');
  e.bomb_recent = true; e.bomb_site = 'A'; assert (EvaluateCTState (e).phase == CTPhase::Cautious);
  e.bomb_site = 'B'; assert (EvaluateCTState (e).phase == CTPhase::Commit);
  e.planted = true; e.planted_site = 'A'; assert (EvaluateCTState (e).phase == CTPhase::Retake && EvaluateCTState (e).site == 'A');
  e.planted_site = '-'; assert (EvaluateCTState (e).phase == CTPhase::LocateBomb);
  e = {}; e.deployed = true; e.sightings_a = 1; e.deaths_a = 1;
  assert (EvaluateCTState (e).phase == CTPhase::Cautious && EvaluateCTState (e).site == 'A');
  const char first[5] { 'B','M','S','S','L' }, second[5] { 'B','M','A','L','L' };
  const char all_a[5] { 'A','A','A','A','A' }, all_b[5] { 'B','B','B','B','B' }, cautious[5] { 'B','B','B','A','M' };
  assert (ValidateCTPlacement (CTPhase::Opening, '-', 1, first, false));
  assert (ValidateCTPlacement (CTPhase::Opening, '-', 2, second, false));
  assert (!ValidateCTPlacement (CTPhase::Opening, '-', 1, all_a, false));
  assert (ValidateCTPlacement (CTPhase::Cautious, 'B', 0, cautious, false));
  assert (ValidateCTPlacement (CTPhase::Commit, 'A', 0, all_a, false));
  assert (ValidateCTPlacement (CTPhase::Retake, 'B', 0, all_b, false));
  assert (!ValidateCTPlacement (CTPhase::Retake, 'B', 0, all_a, false));
  assert (ValidateCTPlacement (CTPhase::Gather, '-', 0, all_a, true));
  assert (!ValidateCTPlacement (CTPhase::Gather, '-', 0, all_a, false));
  puts ("Native CT policy assertions passed.");
}
