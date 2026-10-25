// Real paid API checks, invoked explicitly; never advances or kills a live game round.
import {fixture} from './fixture.mjs';
import {decide,plansFor,wireReply} from './decision.mjs';
import {writeFileSync,mkdirSync} from 'node:fs';
const cases=[];
function add(name,team,setup=()=>{}){const s=fixture(team);s.server_session=999;s.round_state.elapsed=10;Object.assign(s.round_state,{ct_alive:5,t_alive:5});setup(s);cases.push({name,state:s});}
add('ct_pistol','CT',s=>s.t_plan.pistol_round=true);
add('ct_early_a_warning','CT',s=>Object.assign(s.round_state,{phase_code:1,deployed:true,sightings_a:1}));
add('ct_heard_a_rush','CT',s=>{Object.assign(s.round_state,{phase_code:2,deployed:true,heard_a:2});s.sound_reports=[{reporter_id:'ct_01',kind:'footsteps',age:1,site:'A',origin:[1280,2304,128],source:'native_team_hearing',approximate:true}];});
add('ct_overrun_b','CT',s=>Object.assign(s.round_state,{site_lost_b:true,intruders_b:2,deployed:true}));
add('ct_retake_a','CT',s=>{s.bomb_planted=true;s.bomb={planted:true,known:true,site:'A',source:'team_hearing'};s.round_state.planted_site='A';});
add('ct_repeat_long','CT',s=>{s.t_plan.pistol_round=true;s.ct_tendencies={observed_previous_rounds:3,approaches:{long:{three_plus_rounds:3,early_three_plus_rounds:3,stack_recommended:true}}};});
add('t_pistol','T',s=>s.t_plan.pistol_round=true);
add('t_delayed_gun','T',s=>{s.t_plan.pistol_round=false;s.t_plan.rush_allowed=false;s.economy={ready:true,mode:'full',average:4500};});
add('t_recovered_bomb','T',s=>{s.t_plan={locked:true,target_site:'A',attack_delay:45,attack_at:60,stage:2,replanning:true,retargeted:true,elapsed:20};s.round_state.phase_code=13;s.bots[3].carrying_c4=true;});
add('t_dropped_bomb','T',s=>{s.t_plan={locked:true,target_site:'A',attack_delay:45,attack_at:60,stage:2,elapsed:20};s.round_state.phase_code=10;s.bomb_objective={dropped:true,collector_id:'t_04',pickup_position:'recover_0'};for(let i=0;i<5;i++)s.positions.push({id:`recover_${i}`,site:'M',zone:'R',stage:-1,origin:[i*100,1500,36],look_at:[0,1500,36],radius:32,stance:'stand',preferred_roles:'support,entry,lurker,awper',waypoint:2000+i});});
const report={checked_at:new Date().toISOString(),model:'perplexity/pplx-decider-v1.2',timeout_ms:1800,cases:[]};
for(const {name,state} of cases){const start=performance.now();try{
 const offered=Object.keys(plansFor(state)),result=await decide(state,{apiKey:process.env.OPENROUTER_API_KEY});
 wireReply(state,result.plan);if(!result.model?.startsWith(report.model))throw Error('unexpected model');
 report.cases.push({name,team:state.team,ok:true,offered,choice:result.choice,model:result.model,latency_ms:Math.round(performance.now()-start)});
}catch(e){report.cases.push({name,ok:false,error:e.message,latency_ms:Math.round(performance.now()-start)});}}
report.passed=report.cases.every(c=>c.ok);mkdirSync(new URL('./logs/',import.meta.url),{recursive:true});writeFileSync(new URL('./logs/model-verification.json',import.meta.url),JSON.stringify(report,null,2));console.log(JSON.stringify(report));if(!report.passed)process.exitCode=1;
