import {test} from 'node:test';
import assert from 'node:assert/strict';
import {CTTendencies} from './ct-intel.mjs';
import {fixture} from './fixture.mjs';
import {plansFor,validatePlan,requestFor} from './decision.mjs';
import {assessCT} from './round-policy.mjs';
const s=()=>{const x=fixture('CT');x.server_session=1;x.round_state.elapsed=10;x.game_time=20;return x;};
const contact=(id,age=1)=>({enemy_id:id,age,time:19,site:'A',origin:[1459.56,1261.02,36.03]});
test('two previous observed early three-player Long attacks offer a four-player stack without hidden state',()=>{
 const history=new CTTendencies(),x=s();
 for(const round of [1,2]){x.round=round;x.observed_contacts=[1,2,3].map(i=>contact(i));history.observe(x);history.observe(x);}
 x.round=3;x.observed_contacts=[];x.ct_tendencies=history.observe(x);x.t_plan.pistol_round=true;
 assert.equal(x.ct_tendencies.approaches.long.early_three_plus_rounds,2);
 const plans=plansFor(x),stack=plans.tendency_stack_long_4;assert.ok(stack);validatePlan(x,stack);
 assert.equal(Object.values(stack.assignments).filter(id=>x.positions.find(p=>p.id===id).crunch_group==='L').length,4);
 assert.match(requestFor(x).state.briefing,/prediction, not a current sighting/);
 x.round_state.elapsed=26;assert.ok(!plansFor(x).tendency_stack_long_4);
 x.server_session=2;assert.equal(history.observe(x).observed_previous_rounds,0);
});
test('duplicates, stale sightings and a single previous round cannot justify an early stack',()=>{
 const history=new CTTendencies(),x=s();x.observed_contacts=[contact(1),contact(1),contact(2,9),contact(3,9)];history.observe(x);
 x.round=2;x.observed_contacts=[];x.ct_tendencies=history.observe(x);assert.ok(!plansFor(x).tendency_stack_long_4);
});
test('single-site warning permits preemptive reinforcement but not an automatic full rotation',()=>{
 const x=s();Object.assign(x.round_state,{deployed:true,phase_code:1,sightings_a:1});
 const plan=plansFor(x).preempt_reinforce_A;assert.ok(plan);validatePlan(x,plan);assert.equal(assessCT(x).phase,1);
 x.bomb_planted=true;x.round_state.planted_site='B';assert.ok(!plansFor(x).preempt_reinforce_A);
});
test('overrun-site report commits survivors immediately and current pressure wins over history',()=>{
 const x=s();Object.assign(x.round_state,{site_lost_b:true,intruders_b:2,deployed:true});
 assert.deepEqual(assessCT(x),{phase:3,site:'B'});
 for(const plan of Object.values(plansFor(x)))assert.ok(Object.values(plan.assignments).every(id=>x.positions.find(p=>p.id===id).site==='B'));
});
