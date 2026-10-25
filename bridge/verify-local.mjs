import {readFileSync,writeFileSync} from 'node:fs';
import {dirname,resolve,join} from 'node:path';
import {fileURLToPath} from 'node:url';
const bridge=dirname(fileURLToPath(import.meta.url));
const workspace=resolve(bridge,'../..');
const rows=readFileSync(join(bridge,'logs','decisions.jsonl'),'utf8').trim().split(/\r?\n/).filter(Boolean).map(JSON.parse);
const game=readFileSync(join(workspace,'work/runtime/Half-Life/qconsole.log'),'utf8');
const rounds=[];
let current;
for(const line of game.split(/\r?\n/)) {
  let match=line.match(/^\[Jev\] Round (\d+) deployment source=(jev|fallback): A=(\d+),(\d+),(\d+)/);
  if(match){current={request:+match[1],source:match[2],a:match.slice(3).map(Number),chat:[]};rounds.push(current);continue;}
  match=line.match(/^\[Jev\] CT (\d+) says: Going ([AB])/);
  if(match && current)current.chat.push({id:+match[1],site:match[2]});
}
function chatMatches(round,plan) {
  return round?.chat.length===5 && new Set(round.chat.map(x=>x.id)).size===5 &&
    round.chat.every(x=>(x.site==='A'?plan.a:plan.b).includes(x.id)) &&
    round.chat.filter(x=>x.site==='A').length===3;
}
const decisions=rows.filter(x=>x.event==='decision');
const passed=[];
for(const decision of decisions) {
  const round=rounds.findLast(r=>r.source==='jev'&&r.request===decision.request&&JSON.stringify(r.a)===JSON.stringify(decision.plan.a));
  const telemetry=rows.filter(x=>x.event==='telemetry'&&x.state.request===decision.request&&x.state.nonce===decision.nonce);
  const arrival=telemetry.find(x=>x.state.bots.length===5 && x.state.bots.every(b=>{
    const expected=decision.plan.a.includes(b.id)?'A':'B';
    const site=x.state.sites[expected];
    return b.health>0 && b.site===expected && Math.hypot(...b.origin.map((v,i)=>v-site[i]))<300;
  }));
  if(chatMatches(round,decision.plan)&&arrival)passed.push({request:decision.request,nonce:decision.nonce,
    model:decision.model,latency_ms:decision.latency_ms,plan:decision.plan,
    announcements:round.chat,arrival_time:arrival.time,positions:arrival.state.bots});
}
const fallbacks=rounds.filter(r=>r.source==='fallback'&&r.a.join(',')==='1,2,3'&&chatMatches(r,{a:[1,2,3],b:[4,5]}));
const report={checked_at:new Date().toISOString(),live_rounds_with_decisions:decisions.length,
  completed_live_rounds:passed.length,verified_live_rounds:passed,
  fallback_rounds_with_matching_announcements:fallbacks.map(r=>r.request),
  passed:passed.length>0&&fallbacks.length>0};
const output=process.argv[2];
if(output)writeFileSync(resolve(output),JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({passed:report.passed,completed_live_rounds:passed.length,
  fallback_rounds:report.fallback_rounds_with_matching_announcements,
  latest_verified:passed.at(-1)?.plan},null,2));
if(!report.passed)process.exitCode=1;
