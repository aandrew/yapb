import {test} from 'node:test';
import assert from 'node:assert/strict';
import {fixture} from './fixture.mjs';
import {plansFor,validatePlan} from './decision.mjs';
import {loadConfig} from './config.mjs';
test('one fake holds outside the opposite site and rejoins the locked execute',()=>{
 for(const site of ['A','B']){
  const s=fixture('T');s.t_plan={locked:true,target_site:site,attack_delay:60,attack_at:68,elapsed:20,stage:2,fake_id:'t_02'};s.round_state.phase_code=7;
  for(const plan of Object.values(plansFor(s))){
   validatePlan(s,plan);const assigned=s.bots.map(b=>s.positions.find(p=>p.id===plan.assignments[b.id]));
   assert.equal(assigned.filter(p=>p.site!==site).length,1);
   const fake=assigned[1];assert.equal(fake.id,`t_${site==='A'?'b':'a'}_1_entry`);assert.equal(fake.stage,1);assert.ok(['X','Y'].includes(fake.zone));
   const bad=structuredClone(plan);bad.assignments.t_02=site==='A'?'b_anchor':'a_anchor';assert.throws(()=>validatePlan(s,bad));
   const two=structuredClone(plan);two.assignments.t_03=site==='A'?'t_b_0_entry':'t_a_0_entry';assert.throws(()=>validatePlan(s,two));
  }
  s.round_state.phase_code=8;s.t_plan.elapsed=61;
  for(const p of Object.values(plansFor(s)))assert.ok(Object.values(p.assignments).every(id=>s.positions.find(p=>p.id===id).zone===site));
 }
});
test('entry remains a rusher disposition rather than a mandatory front slot',()=>{
 const {roster}=loadConfig();for(const team of [roster.bots,roster.terrorists])assert.equal(team.find(b=>b.role==='entry').personality,1);
 const s=fixture('T');const p=Object.values(plansFor(s)).find(p=>p.timing>0);[p.assignments.t_03,p.assignments.t_04]=[p.assignments.t_04,p.assignments.t_03];validatePlan(s,p);
});

test('recent enemy losses offer early execution without inventing a weaker site',()=>{
 const s=fixture('T');s.game_time=30;s.t_plan={locked:true,target_site:'A',attack_delay:60,attack_at:68,elapsed:22,stage:2};
 Object.assign(s.round_state,{phase_code:7,ct_alive:4,t_alive:5});s.round_events=[{type:'death',time:25,victim_team:'C'}];
 const p=plansFor(s).advantage_execute_now;assert.ok(p);validatePlan(s,p);assert.equal(p.phase,8);assert.equal(p.target_site,'A');assert.equal(p.timing,60);
 s.round_state.t_alive=4;assert.ok(!plansFor(s).advantage_execute_now);
 s.round_state.t_alive=5;s.round_events[0].time=17;assert.ok(!plansFor(s).advantage_execute_now);
 assert.throws(()=>validatePlan(s,p));
});
test('two recent T deaths permit one site switch, keeping the execute clock',()=>{
 const s=fixture('T');s.game_time=30;s.t_plan={locked:true,target_site:'A',attack_delay:60,attack_at:68,elapsed:22,stage:1};
 Object.assign(s.round_state,{phase_code:7,ct_alive:5,t_alive:3});s.round_events=[24,25].map(time=>({type:'death',time,victim_team:'T'}));
 const p=plansFor(s).casualties_switch_site;assert.ok(p);validatePlan(s,p);assert.equal(p.target_site,'B');assert.equal(p.formation,2);assert.equal(p.timing,60);
 assert.ok(Object.values(p.assignments).every(id=>s.positions.find(p=>p.id===id).zone==='Y'));
 s.t_plan.retargeted=true;assert.ok(!plansFor(s).casualties_switch_site);assert.throws(()=>validatePlan(s,p));
 s.t_plan.retargeted=false;s.round_state.phase_code=8;s.t_plan.stage=2;assert.equal(plansFor(s).casualties_switch_site.phase,8);
 s.bomb_planted=true;s.round_state.phase_code=9;assert.ok(!plansFor(s).casualties_switch_site);
});
