// Standalone policy checks use the same header as the game DLL.
#include <assert.h>
#include <stdio.h>
#include "ctstrategy.h"
using namespace bot;
int main () {
  assert(!CrunchMixupAllowed(1,0,0,-100));
  assert(CrunchMixupAllowed(2,0,1,-100));
  assert(!CrunchMixupAllowed(3,0,2,2));
  assert(CrunchMixupAllowed(4,0,3,2));
  assert(!CrunchMixupAllowed(2,1,0,-100));
  assert(CrunchMixupAllowed(4,2,1,-100));
  assert(!CrunchMixupAllowed(5,3,1,4));
  CTBombKnowledge shared {};
  assert (shared.Site () == '-');
  shared.cleared_a = true; assert (shared.Site () == 'B');
  shared.cleared_b = true; assert (shared.Site () == '-');
  shared.confirmed_site = 'A'; assert (shared.Site () == 'A');
  shared = {}; shared.cleared_b = true; assert (shared.Site () == 'A');
  shared.confirmed_site = 'B'; assert (shared.Site () == 'B');
  CTEvidence guard {}; guard.deployed=true; guard.secured_drop=true; guard.dropped_site='M';
  assert(EvaluateCTState(guard).phase==CTPhase::GuardBomb);
  guard.planted=true;guard.planted_site='B';assert(EvaluateCTState(guard).phase==CTPhase::Retake);
  CTEvidence crunch {};crunch.deployed=true;crunch.crunch_active=true;crunch.crunch_choke='L';crunch.sightings_a=2;
  assert(EvaluateCTState(crunch).phase==CTPhase::Crunch);
  crunch.bomb_recent=true;crunch.bomb_site='A';assert(EvaluateCTState(crunch).phase==CTPhase::Commit);
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
