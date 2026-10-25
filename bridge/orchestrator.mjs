import {TOpeningHistory} from './t-opening-history.mjs';
import {CTTendencies} from './ct-intel.mjs';
import {areaFor} from './areas.mjs';
const token=/^[a-z][a-z0-9_]{0,40}$/;
const vector=v=>Array.isArray(v)&&v.length===3&&v.every(n=>Number.isFinite(n)&&Math.abs(n)<100000);
const integer=n=>Number.isSafeInteger(n)&&n>=0;
const side=t=>t==='CT'?'C':'T';
const location=v=>{const a=areaFor(v);return {area:a?.id??'unmapped',label:a?.label??'Unmapped',origin:v};};
export function validateCombatReport(packet){
 const e=packet?.combat;
 if(packet?.version!==1||packet.event!=='combat_report'||!['CT','T'].includes(packet.team)||!token.test(packet.map)||!integer(packet.server_session)||!integer(packet.round)||
  !e||!integer(e.id)||e.id===0||!Number.isFinite(e.time)||e.time<0||e.type!=='death'||!integer(e.killer)||!integer(e.victim)||!['C','T','-'].includes(e.killer_team)||!['C','T'].includes(e.victim_team))throw Error('invalid combat report');
 for(const field of ['origin','killer_origin','observed_victim_origin'])if(e[field]!==undefined&&!vector(e[field]))throw Error('invalid report position');
 const own=side(packet.team);
 if(e.origin&&e.victim_team!==own)throw Error('enemy death position is not a team report');
 if((e.killer_origin||e.observed_victim_origin)&&e.killer_team!==own)throw Error('enemy shooter position is not a team report');
 for(const field of ['killer_identity','victim_identity'])if(e[field]!==undefined&&!token.test(e[field]))throw Error('invalid reporting identity');
 if(e.observed_victim_origin&&!['shooter_visible','recent_team_sighting'].includes(e.observed_source))throw Error('missing observation provenance');
 return packet;
}
export function validateWorldState(packet){
 if(packet?.version!==1||packet.event!=='world_state'||packet.identity!=='silent_orchestrator'||!integer(packet.server_session)||!integer(packet.round)||!token.test(packet.map)||!Number.isFinite(packet.game_time)||
  !Array.isArray(packet.players)||packet.players.length>32||new Set(packet.players.map(p=>p.userid)).size!==packet.players.length||packet.players.some(p=>!token.test(p.id)||!integer(p.userid)||!['CT','T'].includes(p.team)||typeof p.alive!=='boolean'||!vector(p.origin)))throw Error('invalid world state');
 return packet;
}
function reportsForEvent(record){
 const {combat:e,team,round}=record,own=side(team),reports=[];
 if(e.killer_team===own&&e.killer_origin)reports.push({id:`${round}:${e.id}:kill`,round,event_id:e.id,time:e.time,type:'kill',reporter_id:e.killer_identity,position:location(e.killer_origin),victim_userid:e.victim,victim_team:e.victim_team,friendly_fire:e.victim_team===e.killer_team,
  target:e.observed_victim_origin?{...location(e.observed_victim_origin),site:e.observed_site,source:e.observed_source}:null});
 if(e.victim_team===own&&e.origin)reports.push({id:`${round}:${e.id}:death`,round,event_id:e.id,time:e.time,type:'death',reporter_id:e.victim_identity,position:location(e.origin),killer_userid:e.killer,killer_team:e.killer_team,source:'own_death_event'});
 return reports;
}
export function siteIntel(state){
 const result={};
 for(const site of ['A','B']){
  const ids=new Set((state.observed_contacts??[]).filter(c=>{
   const age=Number.isFinite(c.age)?c.age:state.game_time-c.time;
   return age>=0&&age<=8&&state.positions.some(p=>p.zone===site&&Math.abs(c.origin[2]-p.origin[2])<=96&&Math.hypot(c.origin[0]-p.origin[0],c.origin[1]-p.origin[1])<=450);
  }).map(c=>c.enemy_id));
  const losses=(state.round_events??[]).filter(e=>e.victim_team==='C'&&e.killer_team==='T'&&e.observed_site===site&&state.game_time-e.time>=0&&state.game_time-e.time<=20).length;
  result[site]={confirmed_defenders:ids.size,confirmed_defender_losses:losses,status:ids.size?'defenders_observed':losses?'recent_defender_losses':'unknown',unknown_is_not_clear:true};
 }
 return result;
}
export class SilentOrchestrator {
 constructor({now=Date.now}={}){this.now=now;this.world=null;this.records=new Map();this.callers=new Map();this.ledger=new Map();this.ctHistory=new CTTendencies();this.tHistory=new TOpeningHistory();}
 ingestWorld(packet){
  validateWorldState(packet);
  if(this.world&&(this.world.server_session!==packet.server_session||this.world.map!==packet.map)){this.records.clear();this.callers.clear();this.ledger.clear();}
  this.world={...structuredClone(packet),received_at:this.now()};return this.world;
 }
 ingestCombat(packet){
  validateCombatReport(packet);
  if(this.world&&(this.world.server_session!==packet.server_session||this.world.map!==packet.map))return false;
  const key=`${packet.server_session}:${packet.map}:${packet.team}:${packet.round}:${packet.combat.id}`;
  const existing=this.records.get(key);
  this.records.set(key,{...structuredClone(packet),received_at:existing?.received_at??this.now()});
  while(this.records.size>512)this.records.delete(this.records.keys().next().value);
  const e=packet.combat,globalKey=`${packet.server_session}:${packet.round}:${e.id}`;
  const global=this.ledger.get(globalKey)??{round:packet.round,event_id:e.id,killer:e.killer,victim:e.victim,killer_team:e.killer_team,victim_team:e.victim_team,time:e.time};
  if(e.killer_origin)global.killer_position=location(e.killer_origin);
  if(e.origin)global.death_position=location(e.origin);
  this.ledger.set(globalKey,global);while(this.ledger.size>256)this.ledger.delete(this.ledger.keys().next().value);
  return !existing;
 }
 callerView(state){
  const session=state.server_session??0;
  if(this.world&&(this.world.server_session!==session||this.world.map!==state.map))throw Error('stale caller session');
  for(const combat of state.round_events??[]){
   this.ingestCombat({version:1,event:'combat_report',server_session:session,map:state.map,team:state.team,round:state.round,combat});
  }
  const records=[...this.records.values()].filter(r=>r.server_session===session&&r.map===state.map&&r.team===state.team);
  const current=records.filter(r=>r.round===state.round).sort((a,b)=>a.combat.id-b.combat.id);
  const events=current.map(r=>({...r.combat,...(r.combat.origin?{victim_location:location(r.combat.origin)}:{}),...(r.combat.killer_origin?{killer_location:location(r.combat.killer_origin)}:{})}));
  const recent=records.filter(r=>r.round!==state.round&&this.now()-r.received_at<=90000).flatMap(reportsForEvent).slice(-20);
  const own=state.team==='CT'?state.round_state.ct_alive:state.round_state.t_alive,enemy=state.team==='CT'?state.round_state.t_alive:state.round_state.ct_alive;
  const view={...state,caller_context:{identity:`caller_${state.team}`,speaker_id:state.bots.find(b=>b.caller)?.id??null,perspective:'team_observations_and_reports'},round_events:events,
   combat_reports:current.flatMap(reportsForEvent),recent_round_reports:recent,team_numbers:{own_alive:own,enemy_alive:enemy,man_advantage:own>enemy,even:own===enemy}};
  view.site_intel=siteIntel(view);
  if(view.team==='CT')view.ct_tendencies=this.ctHistory.observe(view);
  if(view.team==='T')view.opening_history=this.tHistory.observe(view);
  view.orchestrator_status={source:'silent_orchestrator',knowledge:'team_reports_and_public_status',numbers:view.team_numbers,
   objective:state.bomb_planted?'retake_or_defend_plant':state.bomb_objective?.dropped?(state.team==='T'?'recover_bomb_together':'protect_observed_dropped_bomb'):state.t_plan?.replanning?'reassess_attack_site':'site_strategy',
   sites:view.site_intel,report_count:view.combat_reports.length,
   guidance:state.bomb_objective?.dropped?(state.team==='T'?'Recover C4 before choosing the next site.':'Protect the observed bomb; reform site coverage when picked up.'):'Use confirmed observations and defender losses to compare sites; unknown is not empty.'};
  this.callers.set(state.team,structuredClone(view));return view;
 }
 snapshot(){return this.world?{identity:'silent_orchestrator',omniscient:true,state:this.world,combat_events:[...this.ledger.values()].slice(-64)}:null;}
 caller(team){return this.callers.get(team)??null;}
 health(){return {identity:'silent_orchestrator',world_received:!!this.world,world_age_ms:this.world?this.now()-this.world.received_at:null,callers:[...this.callers.keys()],reports:this.records.size};}
}
