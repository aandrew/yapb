import {test} from 'node:test';
import assert from 'node:assert/strict';
import {fixture} from './fixture.mjs';
import {plansFor,validatePlan} from './decision.mjs';
function objective(s,zone){
 for(let i=0;i<5;i++)s.positions.push({id:`objective_${i}`,site:'M',zone,stage:-1,origin:[i*100,1400,36],look_at:[0,1400,36],radius:32,stance:'stand',preferred_roles:'support,entry,lurker,awper',waypoint:2000+i});
}
test('T bomb recovery excludes attack/fake plans and pins the collector',()=>{
 const s=fixture('T');objective(s,'R');s.round_state.phase_code=10;s.t_plan={locked:true,target_site:'A',attack_delay:60,attack_at:68,elapsed:30,stage:2};
 s.bomb_objective={dropped:true,collector_id:'t_04',pickup_position:'objective_0'};
 const plans=plansFor(s);assert.deepEqual(Object.keys(plans),['recover_bomb_first']);const p=plans.recover_bomb_first;
 assert.equal(p.assignments.t_04,'objective_0');assert.ok(Object.values(p.assignments).every(id=>id.startsWith('objective_')));
 const bad=structuredClone(p);[bad.assignments.t_01,bad.assignments.t_04]=[bad.assignments.t_04,bad.assignments.t_01];assert.throws(()=>validatePlan(s,bad));
 s.round_state.phase_code=8;s.bomb_objective.dropped=false;assert.ok(!plansFor(s).recover_bomb_first);
});
test('CTs guard a known dropped bomb, while a plant takes priority',()=>{
 const s=fixture();objective(s,'G');Object.assign(s.round_state,{phase_code:11,deployed:true,dropped_secured:true,dropped_site:'M'});
 const p=plansFor(s).guard_dropped_bomb;assert.ok(p);validatePlan(s,p);assert.ok(Object.values(p.assignments).every(id=>id.startsWith('objective_')));
 s.bomb_planted=true;s.round_state.planted_site='B';assert.ok(!plansFor(s).guard_dropped_bomb);
});
test('all four choke points offer bounded 3/4-player crunches while remaining players hold',()=>{
 const s=fixture();const home=Object.values(plansFor(s))[0].assignments;
 s.bots.forEach(b=>{b.position=home[b.id];b.origin=s.positions.find(p=>p.id===b.position).origin;});
 Object.assign(s.round_state,{deployed:true,phase_code:1,ct_alive:5,elapsed:4,crunch_used:false,crunch_available:true});
 const p=plansFor(s);
 for(const group of ['long','short','mid','b_tunnels'])for(const count of [3,4]){
  const plan=p[`crunch_${group}_${count}`];assert.ok(plan,`${group} ${count}`);validatePlan(s,plan);
  const moving=s.bots.filter(b=>s.positions.find(x=>x.id===plan.assignments[b.id]).crunch_group===plan.target_site);assert.equal(moving.length,count);
  for(const b of s.bots.filter(b=>!moving.includes(b)))assert.equal(plan.assignments[b.id],home[b.id]);
 }
 s.round_state.crunch_available=false;assert.ok(!Object.keys(plansFor(s)).some(k=>k.startsWith('crunch_')));
 s.round_state.crunch_available=true;s.round_state.crunch_used=true;assert.ok(!Object.keys(plansFor(s)).some(k=>k.startsWith('crunch_')));
});
