import {test} from 'node:test';
import assert from 'node:assert/strict';
import {SilentOrchestrator,validateCombatReport} from './orchestrator.mjs';
import {fixture} from './fixture.mjs';
import {plansFor,validatePlan,requestFor} from './decision.mjs';
const event=(over={})=>({id:1,type:'death',time:20,killer:101,victim:201,killer_team:'T',victim_team:'C',observed_site:'-',observed_source:'unknown',...over});
const packet=(team,combat,round=1)=>({version:1,event:'combat_report',server_session:7,map:'de_dust2',team,round,combat});
const world=(session=7)=>({version:1,event:'world_state',identity:'silent_orchestrator',server_session:session,map:'de_dust2',round:1,game_time:21,match:{a:0,b:0},bomb:{planted:false},players:[{id:'ct_01',userid:201,team:'CT',alive:false,origin:[9876,9876,36]},{id:'t_01',userid:101,team:'T',alive:true,origin:[10,20,36]}]});
function state(team='T'){const s=fixture(team);s.server_session=7;s.game_time=21;Object.assign(s.round_state,{ct_alive:4,t_alive:5});return s;}
test('omniscient ledger merges both reports but caller views cannot see enemy-private positions',()=>{
 const o=new SilentOrchestrator();o.ingestWorld(world());
 o.ingestCombat(packet('CT',event({origin:[8765,8765,36],victim_identity:'ct_01'})));
 o.ingestCombat(packet('T',event({killer_origin:[10,20,36],killer_identity:'t_01'})));
 const view=o.callerView(state());assert.equal(view.combat_reports.length,1);assert.equal(view.combat_reports[0].type,'kill');
 assert.ok(!JSON.stringify(view).includes('8765'));assert.ok(!JSON.stringify(view).includes('9876'));
 assert.deepEqual(o.snapshot().combat_events[0].death_position.origin,[8765,8765,36]);assert.equal(view.caller_context.identity,'caller_T');
 assert.ok(view.team_numbers.man_advantage);assert.equal(o.caller('CT'),null);
});
test('quiet kill report reaches the next model state with observation provenance and no duplicates',()=>{
 const o=new SilentOrchestrator();o.ingestWorld(world());
 const e=event({killer_origin:[10,20,36],killer_identity:'t_01',observed_victim_origin:[-1518,2703,36],observed_site:'B',observed_source:'shooter_visible'});
 assert.equal(o.ingestCombat(packet('T',e)),true);assert.equal(o.ingestCombat(packet('T',e)),false);
 const s=state();s.round_events=[e];const view=o.callerView(s);assert.equal(view.combat_reports.length,1);
 assert.equal(view.combat_reports[0].target.site,'B');assert.equal(view.combat_reports[0].target.source,'shooter_visible');
 assert.equal(requestFor(view).state.combat_reports[0].reporter_id,'t_01');assert.equal(view.site_intel.B.confirmed_defender_losses,1);
});
test('final-round death reports survive into next poll as labelled previous-round history',()=>{
 let now=1000;const o=new SilentOrchestrator({now:()=>now});o.ingestWorld(world());
 o.ingestCombat(packet('T',event({killer_team:'C',victim_team:'T',origin:[200,300,36],victim_identity:'t_02'})));
 const s=state();s.round=2;s.game_time=30;const view=o.callerView(s);
 assert.equal(view.round_events.length,0);assert.equal(view.recent_round_reports[0].round,1);assert.equal(view.recent_round_reports[0].type,'death');
 now+=91000;assert.equal(o.callerView(s).recent_round_reports.length,0);
});
test('report transport refuses unobserved enemy positions in a caller report',()=>{
 assert.throws(()=>validateCombatReport(packet('T',event({origin:[1,2,3],victim_identity:'ct_01'}))));
 assert.throws(()=>validateCombatReport(packet('CT',event({killer_origin:[1,2,3],killer_identity:'t_01'}))));
 assert.throws(()=>validateCombatReport(packet('T',event({observed_victim_origin:[1,2,3]}))));
});
test('new server session clears old reports/callers and refuses old caller snapshots',()=>{
 const o=new SilentOrchestrator();o.ingestWorld(world());o.ingestCombat(packet('T',event({killer_origin:[1,2,3],killer_identity:'t_01'})));o.callerView(state());
 o.ingestWorld(world(8));assert.equal(o.health().reports,0);assert.equal(o.caller('T'),null);assert.throws(()=>o.callerView(state()));
 assert.equal(o.ingestCombat(packet('T',event())),false);
});
test('carrier is pinned to the main support route in every stage, even for closest-order or fake selection',()=>{
 for(const site of ['A','B'])for(const stage of [0,1,2]){
  const s=state();s.round_state.phase_code=7;s.t_plan={locked:true,target_site:site,attack_delay:60,attack_at:68,elapsed:22,stage,fake_id:'t_04'};
  s.bots[3].carrying_c4=true;s.bots[3].position=`t_${site.toLowerCase()}_${stage}_lurker`;s.bots[3].origin=s.positions.find(p=>p.id===s.bots[3].position).origin;
  for(const p of Object.values(plansFor(s))){assert.equal(p.assignments.t_04,`t_${site.toLowerCase()}_${stage}_support`);validatePlan(s,p);}
  const p=Object.values(plansFor(s))[0],lurker=Object.entries(p.assignments).find(([,id])=>id.endsWith('_lurker'))[0];
  const bad=structuredClone(p);[bad.assignments.t_04,bad.assignments[lurker]]=[bad.assignments[lurker],bad.assignments.t_04];assert.throws(()=>validatePlan(s,bad));
 }
});
test('after C4 recovery caller can choose either site afresh despite an earlier pivot',()=>{
 const s=state();s.round_state.phase_code=13;s.t_plan={locked:true,target_site:'A',attack_delay:60,attack_at:68,elapsed:22,stage:0,retargeted:true,replanning:true};s.bots[3].carrying_c4=true;
 const plans=plansFor(s);assert.deepEqual(Object.keys(plans),['recover_then_attack_A','recover_then_attack_B']);
 for(const p of Object.values(plans)){validatePlan(s,p);assert.equal(p.phase,7);assert.equal(p.formation,2);assert.equal(p.assignments.t_04,`t_${p.target_site.toLowerCase()}_2_support`);}
 s.game_time=65;for(const p of Object.values(plansFor(s)))assert.equal(p.phase,8);
});
test('unknown sites stay unknown and remaining CTs cover both sites after casualties',()=>{
 const o=new SilentOrchestrator();o.ingestWorld(world());const intel=o.callerView(state()).site_intel;
 assert.equal(intel.A.status,'unknown');assert.equal(intel.B.status,'unknown');assert.ok(intel.A.unknown_is_not_clear);
 const s=state('CT');s.bots.forEach((b,i)=>b.alive=i===1||i===4);Object.assign(s.round_state,{deployed:true,phase_code:1});
 for(const p of Object.values(plansFor(s))){const sites=s.bots.filter(b=>b.alive).map(b=>s.positions.find(x=>x.id===p.assignments[b.id]).site);assert.ok(sites.includes('A')&&sites.includes('B'));}
});

test('map strategy books supply eligible actionable choices and derived caller status',async()=>{
 const {loadStrategies}=await import('./strategies.mjs');
 assert.throws(()=>loadStrategies('../secret','T'));
 assert.equal(loadStrategies('de_unknown','CT').strategies.length,0);
 const o=new SilentOrchestrator(),s=state();s.t_plan.pistol_round=true;
 const view=o.callerView(s),req=requestFor(view);
 assert.equal(view.orchestrator_status.knowledge,'team_reports_and_public_status');
 assert.ok(req.state.strategy_book.strategies.some(s=>s.id==='pistol_rush_a'&&s.choices.includes('rush_A')));
 s.t_plan.pistol_round=false;s.t_plan.rush_allowed=false;
 assert.ok(!requestFor(o.callerView(s)).state.strategy_book.strategies.some(s=>s.rounds==='pistol'));
 const ct=requestFor(o.callerView(state('CT')));
 assert.equal(ct.state.strategy_book.strategies.length,2);
});
