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
  for(int mask=0;mask<32;++mask){
    bool alive[5] {};char zones[5] {};int count=0;for(int i=0;i<5;++i){alive[i]=(mask&(1<<i))!=0;if(alive[i])++count;}
    RemainingCTCoverage(alive,zones);assert(ValidateCTPlacement(CTPhase::Opening,'-',1,zones,false));
    bool a=false,b=false,mid=false;for(int i=0;i<5;++i)if(alive[i]){a|=zones[i]=='S'||zones[i]=='L';b|=zones[i]=='B';mid|=zones[i]=='M';}
    if(count>=2)assert(a&&b);if(count==1)assert(mid);
  }
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
  CTEvidence crunch {};crunch.deployed=true;crunch.crunch_active=true;crunch.crunch_choke='L';
  assert(EvaluateCTState(crunch).phase==CTPhase::Crunch);
  crunch.sightings_a=2;assert(EvaluateCTState(crunch).phase==CTPhase::Cautious);
  crunch.bomb_recent=true;crunch.bomb_site='A';assert(EvaluateCTState(crunch).phase==CTPhase::Commit);
  CTEvidence lost_site {};lost_site.deployed=true;lost_site.intruders_b=3;lost_site.crunch_active=true;lost_site.crunch_choke='L';
  assert(EvaluateCTState(lost_site).phase==CTPhase::Commit&&EvaluateCTState(lost_site).site=='B');
  lost_site.intruders_b=2;lost_site.crunch_active=false;lost_site.sightings_b=2;assert(EvaluateCTState(lost_site).phase==CTPhase::Cautious);
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
  const char split[5] { 'B','B','M','A','L' }, pistol[5] { 'B','B','A','A','M' };
  const char half[5] { 'B','M','S','S','A' }, eco[5] { 'B','B','B','B','A' };
  assert (ValidateCTPlacement (CTPhase::Opening, '-', 3, split, false));
  assert (ValidateCTPlacement (CTPhase::Opening, '-', 4, pistol, false));
  assert (ValidateCTPlacement (CTPhase::Opening, '-', 5, half, false));
  assert (ValidateCTPlacement (CTPhase::Opening, '-', 6, eco, false));
  assert (!ValidateCTPlacement (CTPhase::Opening, '-', 6, split, false));
  e = {}; e.lost_b = true; e.intruders_b = 2;
  assert (EvaluateCTState (e).phase == CTPhase::Commit && EvaluateCTState (e).site == 'B');
  assert (ValidateCTPlacement (CTPhase::Gather, '-', 8, all_a, false) == false);
  const char reinforce[5] { 'A','A','A','B','M' };
  assert (ValidateCTPlacement (CTPhase::Gather, '-', 8, reinforce, false));
  e = {}; e.deployed = true; e.heard_a = 1;
  assert (EvaluateCTState (e).phase == CTPhase::Gather);
  e.heard_a = 2; e.crunch_active = true;
  assert (EvaluateCTState (e).phase == CTPhase::Cautious && EvaluateCTState (e).site == 'A');
  e.planted = true; e.planted_site = 'B';
  assert (EvaluateCTState (e).phase == CTPhase::Retake && EvaluateCTState (e).site == 'B');
  assert (DefuseNeedsApproach (100.0f * 100.0f, false));
  assert (!DefuseNeedsApproach (60.0f * 60.0f, false));
  assert (!DefuseNeedsApproach (100.0f * 100.0f, true));
  assert (KeepDefuseLatch (true, true, true, true, true));
  assert (!KeepDefuseLatch (true, true, true, false, true)); // Interrupted latch releases retry logic.
  assert (!KeepDefuseLatch (true, true, false, true, true)); // Defused/exploded objective releases task.
  assert (!KeepDefuseLatch (true, false, true, true, true)); // Death cannot keep a task locked.
  assert (!KeepDefuseLatch (false, true, true, true, true)); // Disabled system retains upstream behavior.
  assert (!KeepDefuseLatch (true, true, true, true, false)); // Unrelated progress bars are not defuses.
  puts ("Native CT policy assertions passed.");
}
