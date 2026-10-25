import {readFileSync,writeFileSync,existsSync} from 'node:fs';
import {dirname,resolve,join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {validatePlan} from './decision.mjs';
import {loadConfig} from './config.mjs';
import {areas} from './areas.mjs';
const bridge=dirname(fileURLToPath(import.meta.url)),workspace=resolve(bridge,'../..');
const rows=readFileSync(join(bridge,'logs/decisions.jsonl'),'utf8').trim().split(/\r?\n/).filter(Boolean).map(JSON.parse);
const game=readFileSync(join(workspace,'work/runtime/Half-Life/qconsole.log'),'utf8');
const requests=rows.filter(r=>r.event==='request'&&r.state?.version===2);
const native=new Map(),chats=[],bindings=[],timeouts=new Set();
let current=null,boot=0;
for(const line of game.split(/\r?\n/)){
  if(/YaPB v.*successfully loaded/.test(line))boot++;
  const timeout=line.match(/^\[Jev\] Request (\d+) timed out; retaining last valid named orders/);
  if(timeout)timeouts.add(`${boot}:${timeout[1]}`);
  let m=line.match(/^\[Jev\] Request round=(\d+) request=(\d+)(?: nonce=(\d+))? revision=(\d+) event=(\w+)/);
  if(m){current={boot,round:+m[1],request:+m[2],nonce:m[3]?+m[3]:null,revision:+m[4],event:m[5],orders:{}};native.set(`${boot}:${m[1]}:${m[2]}`,current);continue;}
  m=line.match(/^\[Jev\] Order round=(\d+) request=(\d+) source=(jev|fallback|operator) identity=(\w+) role=(\w+) position=(\w+) site=([AB])/);
  if(m&&m[3]!=='operator'){
    const entry=native.get(`${boot}:${m[1]}:${m[2]}`);
    if(entry){entry.source=m[3];entry.orders[m[4]]={position:m[6],site:m[7],role:m[5]};}
  }
  m=line.match(/^\[Jev\] Chat round=(\d+) identity=(\w+) role=(\w+) site=([AB]) position=(\w+)/);
  if(m)chats.push({boot,round:+m[1],id:m[2],role:m[3],site:m[4],position:m[5]});
  m=line.match(/^\[Jev\] Binding (\w+) role=(\w+) slot=(\d+) userid=(\d+) generation=(\d+)/);
  if(m)bindings.push({boot,id:m[1],role:m[2],slot:+m[3],userid:+m[4],generation:+m[5]});
}
const {roster}=loadConfig();
const valid=[],arrivals=[],announced=[];
for(const decision of rows.filter(r=>r.event==='decision'&&r.plan?.assignments)){
  const request=requests.findLast(r=>r.state.request===decision.request&&r.state.nonce===decision.nonce&&r.state.round===decision.round);
  if(!request)continue;
  const state=request.state;
  try{validatePlan(state,decision.plan);}catch{continue;}
  const entry=[...native.values()].findLast(n=>n.round===state.round&&n.request===state.request&&n.nonce===state.nonce&&n.source==='jev');
  if(!entry||Object.keys(entry.orders).length!==5||state.bots.some(b=>entry.orders[b.id]?.position!==decision.plan.assignments[b.id]))continue;
  valid.push({round:state.round,request:state.request,nonce:state.nonce,event:state.event,game_time:state.game_time,
    model:decision.model,choice:decision.choice,plan:decision.plan,latency_ms:decision.latency_ms});
  if(state.event==='round_start'){
    const chat=chats.filter(c=>c.boot===entry.boot&&c.round===entry.round);
    if(chat.length===5&&new Set(chat.map(c=>c.id)).size===5&&chat.every(c=>c.position===decision.plan.assignments[c.id])&&chat.filter(c=>c.site==='A').length===3)
      announced.push({round:entry.round,nonce:state.nonce,announcements:chat});
  }
  const arrival=rows.find(r=>r.event==='telemetry'&&r.state?.version===2&&r.state.request===state.request&&r.state.nonce===state.nonce&&
    r.state.bots.every(b=>{const target=state.positions.find(p=>p.id===decision.plan.assignments[b.id]);
      return b.alive&&b.position===target.id&&Math.hypot(...b.origin.map((v,i)=>v-target.origin[i]))<120;}));
  if(arrival)arrivals.push({round:state.round,request:state.request,nonce:state.nonce,time:arrival.time,
    bots:arrival.state.bots.map(b=>({id:b.id,role:b.role,slot:b.slot,position:b.position,area:b.area,origin:b.origin}))});
}
const cadence=[];
for(let i=1;i<requests.length;i++){
  const a=requests[i-1].state,b=requests[i].state;
  if(a.round===b.round&&a.roster_revision===b.roster_revision&&b.event==='poll'&&b.request===a.request+1){
    const seconds=+(b.game_time-a.game_time).toFixed(2);
    if(seconds>0&&seconds<20)cadence.push({round:b.round,from:a.request,to:b.request,seconds});
  }
}
const rebindings=roster.bots.map(bot=>({id:bot.id,role:bot.role,
  bindings:bindings.filter(b=>b.id===bot.id&&b.slot>0).map(b=>({boot:b.boot,slot:b.slot,userid:b.userid})),
  role_consistent:bindings.filter(b=>b.id===bot.id).every(b=>b.role===bot.role)}));
const probesPath=join(workspace,'work/protocol-probes.jsonl');
const probes=existsSync(probesPath)?readFileSync(probesPath,'utf8').trim().split(/\r?\n/).filter(Boolean).map(JSON.parse):[];
const rejection=probes.map(p=>{
  const entry=[...native.values()].find(n=>n.round===p.round&&n.request===p.request&&n.revision===p.roster_revision&&
    (n.nonce===null||n.nonce===p.nonce));
  return {phase:p.phase,round:p.round,request:p.request,
    rejected:!!entry&&entry.source!=='jev'&&(timeouts.has(`${entry.boot}:${p.request}`)||entry.source==='fallback')};
});
const report={checked_at:new Date().toISOString(),protocol:2,
  live_decisions_applied:valid.length,live_poll_decisions_applied:valid.filter(v=>v.event==='poll').length,
  latest_decisions:valid.slice(-12),poll_intervals:cadence.slice(-20),
  ten_second_intervals_verified:cadence.filter(c=>Math.abs(c.seconds-10)<0.2).length,
  verified_round_start_announcements:announced,verified_named_position_arrivals:arrivals.slice(-10),
  identities:rebindings,protocol_rejection_probes:rejection,
  callouts:{areas:areas.length,with_waypoints:areas.filter(a=>a.waypoint_ids.length).length,status:'draft boundaries; A/B bomb zones verified'},
  passed:valid.some(v=>v.event==='poll')&&cadence.some(c=>Math.abs(c.seconds-10)<0.2)&&announced.length>0&&arrivals.length>0};
if(process.argv[2])writeFileSync(resolve(process.argv[2]),JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({passed:report.passed,live_decisions_applied:report.live_decisions_applied,
  live_poll_decisions_applied:report.live_poll_decisions_applied,ten_second_intervals_verified:report.ten_second_intervals_verified,
  rounds_with_announcements:announced.length,arrival_snapshots:arrivals.length,
  rejected_protocol_probes:rejection.filter(p=>p.rejected).length,callouts:report.callouts},null,2));
if(!report.passed)process.exitCode=1;
