import {test} from 'node:test';
import assert from 'node:assert/strict';
import {fixture} from './fixture.mjs';
import {loadConfig,validateConfig,engineConfig} from './config.mjs';
import {plansFor,requestFor,decodeAnswer,wireReply,decide,validateState} from './decision.mjs';
import {areas,resolveArea,areaFor,enrichState} from './areas.mjs';

test('every candidate has five unique named positions and exactly three A / two B',()=>{
  const s=fixture(), plans=plansFor(s);
  assert.equal(Object.keys(plans).length,10);
  for(const plan of Object.values(plans)){
    assert.deepEqual(Object.keys(plan.assignments).sort(),s.bots.map(b=>b.id).sort());
    const targets=Object.values(plan.assignments).map(id=>s.positions.find(p=>p.id===id));
    assert.equal(new Set(targets.map(p=>p.id)).size,5);
    assert.equal(targets.filter(p=>p.site==='A').length,3);
    assert.equal(targets.filter(p=>p.site==='B').length,2);
  }
});
test('stable identities and roles survive reordered or reused game slots',()=>{
  const s=fixture(), changed=structuredClone(s);
  changed.bots.forEach((b,i)=>{b.slot=15-i;b.userid+=20;b.generation+=4;});
  changed.roster_revision++;
  assert.deepEqual(plansFor(changed),plansFor(s));
  assert.deepEqual(changed.bots.map(b=>[b.id,b.role]),s.bots.map(b=>[b.id,b.role]));
});
test('duplicate bindings, identities and unknown roles are rejected',()=>{
  for(const mutate of [s=>s.bots[1].id=s.bots[0].id,s=>s.bots[1].slot=s.bots[0].slot,s=>s.bots[0].role='invented']){
    const s=fixture();mutate(s);assert.throws(()=>validateState(s));
  }
});
test('polls include hold and tolerate dead / disconnected identities without renumbering',()=>{
  const s=fixture(), plan=Object.values(plansFor(s))[0];
  s.event='poll';s.bots.forEach(b=>b.position=plan.assignments[b.id]);
  s.bots[0].alive=false;s.bots[0].health=0;
  s.bots[1].connected=false;s.bots[1].alive=false;s.bots[1].slot=0;s.bots[1].userid=0;s.bots[1].generation=0;
  assert.deepEqual(plansFor(s).hold,plan);
  assert.equal(Object.keys(plansFor(s)).length,11);
});
test('role preferences assign the AWPer an appropriate angle within a candidate',()=>{
  const s=fixture(), plans=plansFor(s);
  for(const p of Object.values(plans))assert.ok(['a_anchor','b_anchor'].includes(p.assignments.ct_01));
});
test('typed choices become versioned, round/request/revision-bound wire orders',async()=>{
  const s=fixture();s.bots.reverse();
  const key=Object.keys(plansFor(s))[0];
  const result=await decide(s,{apiKey:'test-key',fetchFn:async(url,options)=>{
    assert.equal(url,'https://openrouter.ai/api/alpha/decisions');
    assert.deepEqual(JSON.parse(options.body),requestFor(s));
    return {ok:true,json:async()=>({model:'typesafe/jev-1.14',answers:{deployment:{type:'choice',choice:key}}})};
  }});
  const words=wireReply(s,result.plan).split(' ');
  assert.deepEqual(words.slice(0,5),['YJ2','1','7','123','1']);
  assert.deepEqual(words.slice(5).map(Number),s.bots.map(b=>s.positions.findIndex(p=>p.id===result.plan.assignments[b.id])));
});
test('unknown choices, invented positions and overlapping placements are rejected',()=>{
  const s=fixture();
  assert.throws(()=>decodeAnswer(s,{answers:{deployment:{type:'choice',choice:'invented'}}}));
  const p=structuredClone(Object.values(plansFor(s))[0]);p.assignments.ct_01='imaginary';assert.throws(()=>wireReply(s,p));
  p.assignments.ct_01=p.assignments.ct_02;assert.throws(()=>wireReply(s,p));
});
test('provider errors and timeouts propagate to the engine fallback policy',async()=>{
  await assert.rejects(decide(fixture(),{apiKey:'test-key',fetchFn:async()=>({ok:false,status:429})}),/HTTP 429/);
  await assert.rejects(decide(fixture(),{apiKey:'test-key',fetchFn:async()=>{throw new Error('timeout');}}),/timeout/);
});
test('registry exports safe engine tokens and rejects inconsistent authoring data',()=>{
  const {roster,map}=loadConfig();
  assert.match(engineConfig(roster,map),/bot ct_01 CT_Jev_1 awper/);
  assert.match(engineConfig(roster,map),/position a_anchor A/);
  const bad=structuredClone(map);bad.positions[0].id='bad position';assert.throws(()=>validateConfig(roster,bad));
  const duplicate=structuredClone(roster);duplicate.bots[1].name=duplicate.bots[0].name;assert.throws(()=>validateConfig(duplicate,map));
});

test('reference callouts map to actual graph areas, including common aliases',()=>{
  assert.equal(areas.length,57);
  assert.ok(areas.every(a=>a.waypoint_ids.length>0&&a.waypoint_ids.includes(a.representative_waypoint)));
  assert.equal(resolveArea('Double Doors').id,'mid_doors');
  assert.equal(resolveArea('B Window').id,'b_window');
  assert.equal(areaFor([1152,2464,144]).id,'a_site');
  assert.equal(areaFor([-1536,2688,48]).id,'b_site');
  assert.equal(areaFor([99999,99999,0]),null);
});
test('height separates CT Spawn from Short above it, and unknown positions stay unmapped',()=>{
  assert.equal(areaFor([400,2300,-88]).id,'ct_spawn');
  assert.equal(areaFor([400,2300,164]).id,'short_a');
  const state=fixture(), enriched=enrichState(state);
  assert.equal(enriched.positions[0].area,'a_site');
  assert.deepEqual(state.bots[0].area,undefined);
  assert.deepEqual(enriched.bots.map(b=>b.id),state.bots.map(b=>b.id));
});
