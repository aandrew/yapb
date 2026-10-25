import {readFileSync,writeFileSync} from 'node:fs';
import {dirname,resolve,join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {validatePlan} from './decision.mjs';
const root=resolve(dirname(fileURLToPath(import.meta.url)),'../..');
const rows=readFileSync(join(root,'yapb/bridge/logs/decisions.jsonl'),'utf8').trim().split(/\r?\n/).filter(Boolean).map(JSON.parse);
const key=s=>`${s.round}:${s.request}:${s.nonce}`;
const requests=rows.filter(r=>r.event==='request'&&r.state?.version===3);
const states=rows.filter(r=>r.state?.version===3).map(r=>r.state);
const lookup=new Map(requests.map(r=>[key(r.state),r.state]));
const orders=new Map(),announcements=[];
for(const line of readFileSync(join(root,'work/runtime/Half-Life/qconsole.log'),'utf8').split(/\r?\n/)){
 if(!line.startsWith('[Jev]'))continue;
 const kv=Object.fromEntries([...line.matchAll(/(\w+)=([\w.-]+)/g)].map(m=>[m[1],m[2]]));
 if(line.startsWith('[Jev] Order ')&&kv.nonce&&kv.source==='jev'){
  const k=key(kv);if(!orders.has(k))orders.set(k,{});orders.get(k)[kv.identity]=kv;
 }
 if(line.startsWith('[Jev] Chat '))announcements.push(kv);
}
const applied=[];
for(const d of rows.filter(r=>r.event==='decision'&&r.plan)){
 const s=lookup.get(key(d));if(!s)continue;
 try{validatePlan(s,d.plan);}catch{continue;}
 const native=orders.get(key(d));
 if(!native||!s.bots.every(b=>native[b.id]?.position===d.plan.assignments[b.id]))continue;
 applied.push({team:s.team,round:s.round,request:s.request,nonce:s.nonce,event:s.event,game_time:s.game_time,phase:d.plan.phase_name,site:d.plan.target_site,timing:d.plan.timing,latency_ms:d.latency_ms,assignments:d.plan.assignments});
}
const cadence=[];
for(const team of ['CT','T']){
 const list=requests.filter(r=>r.state.team===team).map(r=>r.state);
 for(let i=1;i<list.length;i++){const a=list[i-1],b=list[i];
  if(a.round===b.round&&a.roster_revision===b.roster_revision&&b.request===a.request+1&&b.event==='poll'){
   const delta=+(b.game_time-a.game_time).toFixed(2);if(Math.abs(delta-5)<.25)cadence.push({team,round:b.round,seconds:delta});
  }
 }
}
const locked=new Map();let clockViolations=0;
for(const s of states.filter(s=>s.team==='T'&&s.t_plan?.locked)){
 // Native game time may restart on a new server boot; attack_at distinguishes those rounds.
 const k=`${s.round}:${s.t_plan.attack_at}`,p=s.t_plan,prev=locked.get(k);
 if(prev&&(prev.delay!==p.attack_delay||(!s.bomb_planted&&prev.site!==p.target_site)))clockViolations++;
 locked.set(k,{site:p.target_site,delay:p.attack_delay});
 if(s.round_state.phase_code===8&&s.game_time+.05<p.attack_at)clockViolations++;
}
const phases=Object.fromEntries(['CT','T'].map(t=>[t,[...new Set(states.filter(s=>s.team===t).map(s=>s.round_state.phase))]]));
const humanStates=states.filter(s=>s.bots.some(b=>b.human));
const counts=Object.fromEntries(['CT','T'].map(t=>[t,applied.filter(d=>d.team===t).length]));
const report={checked_at:new Date().toISOString(),protocol:3,live_decisions_applied:counts,latest_applied:applied.slice(-12),five_second_intervals:cadence.slice(-20),observed_phases:phases,locked_t_rounds:locked.size,clock_violations:clockViolations,round_role_announcements:announcements.slice(-20),human_roster_snapshots:humanStates.length,public_death_snapshots:states.filter(s=>s.round_events?.length).length,planted_snapshots:states.filter(s=>s.bomb_planted).length,limitations:['Human client chat and replacement need interactive playtesting if human_roster_snapshots is zero.','Unobserved phases are covered by policy tests, not live evidence.','Fine callout boundaries and staged routes remain draft.'],passed:counts.CT>0&&counts.T>0&&cadence.some(c=>c.team==='CT')&&cadence.some(c=>c.team==='T')&&clockViolations===0};
if(process.argv[2])writeFileSync(resolve(process.argv[2]),JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({passed:report.passed,applied:counts,phases,five_second_intervals:cadence.length,clock_violations:clockViolations,humans:humanStates.length,planted:report.planted_snapshots},null,2));
process.exitCode=report.passed?0:1;
