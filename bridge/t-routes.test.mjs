import {test} from 'node:test';
import assert from 'node:assert/strict';
import {fixture} from './fixture.mjs';
import {plansFor,validatePlan,requestFor,decodeAnswer} from './decision.mjs';
import {TOpeningHistory} from './t-opening-history.mjs';
test('Short-A and Mid-B are full five-player routes with safe carrier placement through all three stages',()=>{
 for(const [prefix,site,base] of [['short_to_A','A',3],['mid_to_B','B',6]]){
  const s=fixture('T');s.bots[3].carrying_c4=true;s.t_plan.rush_allowed=false;
  const key=Object.keys(plansFor(s)).find(k=>k.startsWith(prefix)),opening=plansFor(s)[key];
  assert.ok(opening);assert.equal(opening.formation,base);validatePlan(s,opening);
  assert.match(requestFor(s).questions.deployment.criteria[key],site==='A'?/Catwalk and Short Stairs/:/Mid Doors and CT Mid/);
  assert.ok(decodeAnswer(s,{answers:{deployment:{type:'choice',choice:key}}}).strategy);
  for(let stage=base;stage<base+3;stage++){
   s.t_plan={locked:true,target_site:site,attack_delay:45,attack_at:60,stage,elapsed:10};s.round_state.phase_code=7;
   for(const plan of Object.values(plansFor(s))){validatePlan(s,plan);assert.equal(plan.formation,stage);assert.equal(plan.assignments.t_04,`t_${site.toLowerCase()}_${stage}_support`);}
  }
 }
});
test('opening history records own allocations and recommends a less repeated available route',()=>{
 const h=new TOpeningHistory(),s=fixture('T');s.server_session=10;
 for(const round of [1,2,3]){s.round=round;s.t_plan={locked:true,target_site:'A',stage:0};h.observe(s);}
 s.round=4;s.t_plan={locked:false};const history=h.observe(s);
 assert.equal(history.counts.long_a,3);assert.notEqual(history.recommended_route,'long_a');
 s.opening_history=history;assert.match(requestFor(s).state.briefing,/avoid predictable repetition/);
 s.server_session=11;assert.equal(h.observe(s).previous.length,0);
});
test('CT briefings use defensive instructions and retain live numbers without an advantage announcement',()=>{
 const s=fixture('CT');Object.assign(s.round_state,{ct_alive:5,t_alive:2});const r=requestFor(s);
 assert.ok(!r.state.briefing.includes('We have a player advantage'));
 assert.match(r.questions.deployment.instructions,/Your job is to defend A and B/);
 assert.ok(!r.questions.deployment.instructions.includes('A confirmed site pick can justify an execute'));
});
