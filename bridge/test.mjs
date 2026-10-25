import {test} from 'node:test';
import assert from 'node:assert/strict';
import {fixture} from './fixture.mjs';
import {plansFor,validatePlan,wireReply,decide,requestFor,validateState} from './decision.mjs';
import {assessCT} from './round-policy.mjs';
import {loadConfig,engineConfig} from './config.mjs';
import {areas,areaFor,resolveArea} from './areas.mjs';
function ct(changes={},planted=false){const s=fixture();Object.assign(s.round_state,{deployed:true},changes);s.bomb_planted=planted;return s;}
test('CT opening offers exactly the requested two five-position formations',()=>{
 const s=fixture(),p=plansFor(s);assert.equal(Object.keys(p).length,2);
 const counts=Object.values(p).map(plan=>{validatePlan(s,plan);const out={};Object.values(plan.assignments).forEach(id=>{const z=s.positions.find(p=>p.id===id).zone;out[z]=(out[z]??0)+1;});return out;});
 assert.ok(counts.some(c=>c.B===1&&c.M===1&&c.S===2&&c.L===1));
 assert.ok(counts.some(c=>c.B===1&&c.M===1&&c.A===1&&c.L===2));
});
test('isolated sightings do not cause wholesale rotations',()=>{
 assert.deepEqual(assessCT(ct({sightings_b:1})),{phase:1,site:'-'});
 assert.deepEqual(assessCT(ct({sightings_b:2})),{phase:2,site:'B'});
 assert.deepEqual(assessCT(ct({sightings_a:1,recent_ct_deaths_a:1})),{phase:2,site:'A'});
});
test('clear attack plus fresh bomb confirmation commits, but wrong-site bomb does not',()=>{
 assert.deepEqual(assessCT(ct({sightings_b:2,bomb_recent:true,bomb_site:'B'})),{phase:3,site:'B'});
 assert.deepEqual(assessCT(ct({sightings_b:2,bomb_recent:true,bomb_site:'A'})),{phase:2,site:'B'});
 for(const site of ['A','B']){
  const s=ct({[site==='A'?'sightings_a':'sightings_b']:2,bomb_recent:true,bomb_site:site});
  for(const plan of Object.values(plansFor(s)))assert.equal(Object.values(plan.assignments).filter(id=>s.positions.find(p=>p.id===id).zone===site).length,5);
 }
});
test('planted bomb takes precedence and sends the whole roster to retake',()=>{
 for(const site of ['A','B']){
  const s=ct({planted_site:site,sightings_a:5},true);
  assert.deepEqual(assessCT(s),{phase:4,site});
  for(const p of Object.values(plansFor(s))){validatePlan(s,p);assert.ok(Object.values(p.assignments).every(id=>s.positions.find(x=>x.id===id).zone===site));}
 }
 assert.deepEqual(assessCT(ct({planted_site:'-'},true)),{phase:5,site:'-'});
});
test('cautious rotations leave one defender opposite and one mid',()=>{
 for(const site of ['A','B']){
  const s=ct({[site==='A'?'sightings_a':'sightings_b']:2});
  for(const p of Object.values(plansFor(s))){const z=Object.values(p.assignments).map(id=>s.positions.find(x=>x.id===id).zone);assert.equal(z.filter(x=>x===site).length,3);assert.equal(z.filter(x=>x==='M').length,1);}
 }
});
test('T caller offers A/B rush or a one-off delay within 30..60 seconds',()=>{
 const s=fixture('T'),plans=plansFor(s);assert.equal(Object.keys(plans).length,6);
 for(const p of Object.values(plans)){validatePlan(s,p);assert.ok(p.timing===0||(p.timing>=30&&p.timing<=60));}
 assert.equal(new Set(Object.values(plans).map(p=>p.target_site)).size,2);
});
test('locked T plan cannot change target or postpone the attack',()=>{
 const s=fixture('T');s.t_plan={locked:true,target_site:'B',attack_delay:65,attack_at:73,elapsed:20,stage:1};s.round_state.phase_code=7;
 const plans=plansFor(s);
 for(const p of Object.values(plans)){
  assert.equal(p.timing,65);assert.equal(p.target_site,'B');assert.equal(p.formation,1);
  assert.ok(Object.values(p.assignments).every(id=>s.positions.find(x=>x.id===id).stage===1));
  assert.throws(()=>validatePlan(s,{...p,timing:80}));assert.throws(()=>validatePlan(s,{...p,target_site:'A'}));
 }
});
test('T execution commits all five and each clearing stage has entry, flank and lurker posts',()=>{
 const s=fixture('T');s.t_plan={locked:true,target_site:'A',attack_delay:60,attack_at:68,elapsed:61,stage:2};s.round_state.phase_code=8;
 for(const p of Object.values(plansFor(s)))assert.ok(Object.values(p.assignments).every(id=>s.positions.find(x=>x.id===id).zone==='A'));
 for(const zone of ['X','Y'])for(const stage of [0,1,2]){const p=s.positions.filter(p=>p.zone===zone&&p.stage===stage);assert.equal(p.length,5);assert.ok(p.some(p=>p.id.endsWith('_flank')));assert.ok(p.some(p=>p.id.endsWith('_lurker')));}
});
test('humans are valid roster members with roles and advisory targets; no special movement commands exist',()=>{
 for(const team of ['CT','T']){const s=fixture(team);s.bots[4]={...s.bots[4],id:team==='CT'?'ct_human_55':'t_human_55',human:true,slot:12,userid:55};
  validateState(s);for(const p of Object.values(plansFor(s)))assert.ok(p.assignments[s.bots[4].id]);}
});
test('binding changes retain stable bot roles and do not alter candidate identities',()=>{
 const s=fixture(),r=structuredClone(s);r.bots.forEach((b,i)=>{b.slot=15-i;b.userid+=30;b.generation+=2;});r.roster_revision++;
 assert.deepEqual(plansFor(r),plansFor(s));
 r.bots[1].slot=r.bots[0].slot;assert.throws(()=>validateState(r));
});
test('wire order includes phase/site/formation/locked timing and request binding',()=>{
 const s=fixture(),p=Object.values(plansFor(s))[0],words=wireReply(s,p).split(' ');
 assert.deepEqual(words.slice(0,9),['YJ3','1','7','123','1','0','-','1','0']);assert.equal(words.length,14);
 const bad=structuredClone(p);bad.assignments.ct_01=bad.assignments.ct_02;assert.throws(()=>wireReply(s,bad));
});
test('provider adapter sends bounded plans and rejects provider errors',async()=>{
 const s=fixture(),choice=Object.keys(plansFor(s))[0];
 const result=await decide(s,{apiKey:'test-key',fetchFn:async(url,o)=>{assert.equal(url,'https://openrouter.ai/api/alpha/decisions');assert.deepEqual(JSON.parse(o.body),requestFor(s));return{ok:true,json:async()=>({answers:{deployment:{type:'choice',choice}}})};}});
 validatePlan(s,result.plan);
 await assert.rejects(decide(s,{apiKey:'test-key',fetchFn:async()=>({ok:false,status:429})}),/HTTP 429/);
});
test('caller is a role overlay on both rosters and both runtime configurations are generated',()=>{
 const {roster,map}=loadConfig();for(const members of [roster.bots,roster.terrorists]){assert.equal(members.filter(b=>b.caller).length,1);assert.equal(members.filter(b=>b.role==='support').length,2);}
 assert.match(engineConfig(roster,map,'T'),/bot t_04 T_Jev_4 support 2 0 1/);
});
test('callout areas distinguish floors and retain aliases',()=>{
 assert.equal(areas.length,57);assert.equal(resolveArea('B Window').id,'b_window');
 assert.equal(areaFor([400,2300,-88]).id,'ct_spawn');assert.equal(areaFor([400,2300,164]).id,'short_a');
});

 test('CT strategy categories offer supported matching formations and T has twelve grounded entries',async()=>{
  const {loadStrategies,strategyContext}=await import('./strategies.mjs');
  assert.equal(loadStrategies('de_dust2','T').strategies.length,12);
  for(const [category,pistol,mode,key,formation] of [
   ['pistol',true,'save','two_b_two_a_ct',4],['buy',false,'full','two_b_mid_a_long',3],
   ['half_buy',false,'half','b_mid_two_close_short_a',5],['save',false,'save','four_b_one_a',6]]){
   const s=fixture('CT');s.t_plan.pistol_round=pistol;s.economy={ready:true,mode};
   const plans=plansFor(s);assert.equal(plans[key].formation,formation);validatePlan(s,plans[key]);
   const book=strategyContext(s,plans);assert.equal(book.round_category,category);
   assert.ok(book.strategies.some(x=>x.choices.includes(key)));
   assert.ok(book.strategies.every(x=>x.rounds===category));
  }
 });

test('opening reply carries the applicable strategy name safely for one native announcement',async()=>{
 const {decodeAnswer}=await import('./decision.mjs');
 const s=fixture('CT');s.t_plan.pistol_round=true;
 const result=decodeAnswer(s,{answers:{deployment:{type:'choice',choice:'two_b_two_a_ct'}}});
 assert.equal(result.strategy.id,'passive_pistol');
 assert.ok(wireReply(s,result.plan).endsWith(' |two B two A CT'));
 const t=fixture('T');t.t_plan.pistol_round=false;t.economy={ready:true,mode:'half'};
 const choice=Object.keys(plansFor(t)).find(k=>k.startsWith('clear_then_A'));
 const selected=decodeAnswer(t,{answers:{deployment:{type:'choice',choice}}});
 assert.equal(selected.strategy.id,'half_buy_a');
 assert.ok(!wireReply(t,{...selected.plan,strategy_name:'unsafe;"say test\n'}).includes(';'));
});

test('model gets a literal briefing and actionable calls, while raw state stays local',()=>{
 const s=fixture('T');s.t_plan.pistol_round=true;Object.assign(s.round_state,{ct_alive:4,t_alive:4});
 s.bots[3].carrying_c4=true;
 const req=requestFor(s),json=JSON.stringify(req);
 assert.deepEqual(Object.keys(req.state),['briefing']);
 assert.match(req.state.briefing,/Numbers are even/);assert.match(req.state.briefing,/t_04; keep it behind the main group/);
 assert.match(req.questions.deployment.criteria.rush_A,/Attack bombsite A immediately/);
 assert.match(req.questions.deployment.instructions,/Hypothetical examples, not events/);
 for(const key of ['origin','waypoint','phase_code','nonce','roster_revision','positions'])assert.ok(!Object.hasOwn(req.state,key));
 assert.ok(!json.includes('look_at'));assert.ok(!json.includes('preferred_roles'));
 const {briefing}=requestFor(s).state;assert.ok(!briefing.includes('guaranteed current position at'));
});

test('non-rush execute timing accepts both new bounds and rejects outside them',()=>{
 for(const [nonce,delay] of [[0,30],[30,60]]){
  const s=fixture('T');s.nonce=nonce;s.t_plan.rush_allowed=false;
  const plans=plansFor(s);assert.equal(Object.keys(plans).length,4);
  for(const plan of Object.values(plans)){
   assert.equal(plan.timing,delay);validatePlan(s,plan);
   for(const timing of [29,61])assert.throws(()=>validatePlan(s,{...plan,timing}));
  }
 }
});

test('Decider migration retains typed choice validation, errors and a bounded request deadline',async()=>{
 const s=fixture(),choice=Object.keys(plansFor(s))[0];
 assert.equal(requestFor(s).model,'perplexity/pplx-decider-v1.2');
 for(const status of [401,429,500,503])await assert.rejects(()=>decide(s,{apiKey:'test',fetchFn:async()=>({ok:false,status})}),new RegExp(`HTTP ${status}`));
 for(const body of [{answers:{deployment:{type:'choice',choice:'invented'}}},{answers:{deployment:{type:'text',choice}}},{}])await assert.rejects(()=>decide(s,{apiKey:'test',fetchFn:async()=>({ok:true,json:async()=>body})}),/invalid model answer/);
 await assert.rejects(()=>decide(s,{apiKey:'test',fetchFn:async()=>({ok:true,json:async()=>{throw SyntaxError('invalid JSON');}})}),/invalid JSON/);
 await assert.rejects(()=>decide(s,{apiKey:'test',timeout:5,fetchFn:async(_,o)=>new Promise((resolve,reject)=>{
  const keepAlive=setTimeout(()=>reject(Error('deadline did not fire')),100);
  o.signal.addEventListener('abort',()=>{clearTimeout(keepAlive);reject(o.signal.reason);},{once:true});
 })}),e=>e.name==='TimeoutError');
});
