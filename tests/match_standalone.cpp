// SPDX-License-Identifier: Unlicense
#include "match_policy.h"
#include <cassert>
#include <cstdio>
using namespace bot;
int main () {
 MatchRules opening; assert(opening.PistolRound()&&opening.OpeningRushAllowed());
 opening.a=1;assert(!opening.PistolRound()&&!opening.OpeningRushAllowed());
 opening.a=3;assert(!opening.PistolRound()&&opening.OpeningRushAllowed());
 opening.a=6;opening.b=6;assert(opening.PistolRound()&&opening.OpeningRushAllowed());
 opening.overtime=true;assert(!opening.PistolRound()&&!opening.OpeningRushAllowed());
 MatchRules m;
 for(int i=0;i<11;++i) { m.Win (i%2==0); }
 assert (m.MustBuy ());assert (m.Win (false)==MatchAction::Halftime);assert (m.swapped&&m.a==6&&m.b==6);
 for(int i=0;i<11;++i) { m.Win (i%2==0); }
 assert (m.a==12&&m.b==11&&!m.finished&&m.MustBuy ());
 assert (m.Win (false)==MatchAction::Overtime);assert (m.overtime&&!m.finished&&m.a==12&&m.b==12);
 assert (m.Win (true)==MatchAction::None);assert (m.a==13&&!m.finished);
 m.Win (true);assert (m.MustBuy ());assert (m.Win (true)==MatchAction::OvertimeHalf);assert (!m.swapped);
 m.Win (false);m.Win (false);assert (m.Win (false)==MatchAction::NextOvertime);assert (m.overtime_base==15&&m.overtime_rounds==0);
 m.Win (false);m.Win (false);assert (m.Win (false)==MatchAction::OvertimeHalf);
 assert (m.Win (false)==MatchAction::Finished&&m.b==19&&m.finished);
 const bool side=m.swapped;m.Reset ();assert (m.a==0&&m.b==0&&!m.overtime&&!m.finished&&m.swapped==side);
 MatchRules sweep;for(int i=0;i<12;++i){sweep.Win (true);}assert (sweep.swapped&&sweep.MustBuy ());
 assert (sweep.Win (true)==MatchAction::Finished&&sweep.a==13);
 assert (sweep.SideScore (false)==13);assert (sweep.Win (false)==MatchAction::None);
 puts ("MR12, side scores, pistol resets, MR3 ties/repeats and must-buy assertions passed.");
}
