import {strategyContext} from './strategies.mjs';
import {roles} from './config.mjs';
import {ctPlans,validateTacticalPlacement} from './round-policy.mjs';
import {tPlans,validateTPlan} from './t-policy.mjs';
const token=/^[a-z][a-z0-9_]{0,30}$/;
const vector=x=>Array.isArray(x)&&x.length===3&&x.every(v=>Number.isFinite(v)&&Math.abs(v)<100000);
export function validateState(state){
  if(state?.version!==3||!['CT','T'].includes(state.team)||!token.test(state.map)||!['round_start','poll','telemetry'].includes(state.event)||
    ['round','request','nonce','roster_revision'].some(k=>!Number.isSafeInteger(state[k])||state[k]<0)||
    !Number.isFinite(state.game_time)||!Array.isArray(state.bots)||state.bots.length!==5||
    !Array.isArray(state.positions)||state.positions.length<15||state.positions.length>64)throw new Error('invalid state');
  if(new Set(state.bots.map(b=>b.id)).size!==5||state.bots.some(b=>!token.test(b.id)||!roles.includes(b.role)||
    !Number.isInteger(b.slot)||b.slot<0||b.slot>32||!Number.isInteger(b.userid)||b.userid<0||
    !Number.isInteger(b.generation)||b.generation<0||typeof b.alive!=='boolean'||typeof b.connected!=='boolean'||
    !vector(b.origin)||!Number.isFinite(b.health)))throw new Error('invalid roster');
  const connected=state.bots.filter(b=>b.connected);
  if(connected.some(b=>b.slot===0)||new Set(connected.map(b=>b.slot)).size!==connected.length)throw new Error('duplicate game binding');
  if(new Set(state.positions.map(p=>p.id)).size!==state.positions.length||state.positions.some(p=>!token.test(p.id)||
    !['A','B','M'].includes(p.site)||!['A','B','M','S','L','X','Y','G','R'].includes(p.zone)||!vector(p.origin)||!vector(p.look_at)||
    !Number.isInteger(p.waypoint)||p.waypoint<0||!Number.isInteger(p.stage)||!Number.isFinite(p.radius)||p.radius<8||p.radius>96||
    !['stand','crouch'].includes(p.stance)||typeof p.preferred_roles!=='string'||p.preferred_roles.split(',').some(r=>!roles.includes(r))))throw new Error('invalid positions');
  if(!state.round_state||!Number.isInteger(state.round_state.phase_code))throw new Error('missing round policy');
  return state;
}
export function validatePlan(state,plan){
  if(!plan?.assignments||Object.keys(plan.assignments).length!==5)throw new Error('invalid plan');
  return state.team==='T'?validateTPlan(state,plan):validateTacticalPlacement(state,plan);
}
export function plansFor(state){
  validateState(state);
  const plans=state.team==='T'?tPlans(state):ctPlans(state);
  if(state.team==='CT'&&state.round_state.phase_code===0){
    const book=strategyContext(state,plans),allowed=new Set(book.strategies.flatMap(s=>s.choices));
    if(allowed.size)return Object.fromEntries(Object.entries(plans).filter(([key])=>allowed.has(key)));
  }
  return plans;
}
export function requestFor(state,model='typesafe/jev-1.14'){
  const plans=plansFor(state);
  return {model,state:{...state,strategy_book:strategyContext(state,plans)},questions:{deployment:{type:'choice',instructions:state.team==='T'?
    'Prefer a five-player rush on pistol rounds. On other rounds, delayed or reactive attacks are the default. Only use a supplied non-pistol rush as an occasional mix-up. A fresh pick permits immediate execution only when its observed location matches the attack site. A confirmed pick on the other site may justify the supplied optional pivot there. Unknown-location kills must not be treated as site openings. When the bomb is dropped, the whole team recovers it before every site plan. After recovery choose A or B again using site_intel, sightings and confirmed defender losses. Unknown is not empty. The carrier must stay on the main support route with the majority, never lurk or fake. Quiet combat_reports give teammate kill/death locations; the omniscient orchestrator world is separate from this caller view. Supports are preferred bomb carriers and stay behind teammates. A delayed attack runs with guns to ready positions and only sneaks near quiet chokepoints. Once exposed run more. A single designated fake may approach the alternate site but must hold outside and rejoin on execute. Clear in stages: entry has an aggressive disposition without a mandatory lead position, supports trade/watch flanks, lurker trails or takes the alternate route. Keep the original execute clock. Only describe a man advantage when living Ts strictly outnumber living CTs. Equal numbers such as 4v4 are a trade or site pick, not a man advantage. Use confirmed on-site picks for the supplied early execute. After heavy own casualties, consider the supplied one-time site switch. Use actual sightings to judge whether the site is weak, and do not invent defender positions. At attack time commit directly to the site. Humans receive caller advice, never movement control. Use orchestrator_status and the map strategy_book to inform the opening choice. Choose only a supplied complete-team order.':
    'Use orchestrator_status and the map strategy_book to inform the opening choice. Choose a complete CT placement consistent with the supplied round phase. At round start choose a supplied defense from the eligible map strategy book for pistol, buy, half-buy or save circumstances. Conditions such as utility and favorable spawns are preferences, not guaranteed coordinated actions. Standard defense is the default. Only use a crunch as an occasional mix-up, especially when losing. During an eligible early gather phase consider a supplied three- or four-player crunch of Long, Short, Mid or B tunnels. Stay inside the supplied stopping area. After fighting stops the engine returns everyone to their original positions. Gather information without needless movement. Three or more Ts observed inside a bombsite require an immediate full rotation there, even before bomb confirmation. A clear attack on an approach permits cautious reinforcement; a confirmed bomb attack commits everyone; a planted bomb requires all surviving defenders to retake. Use only actual team observations and public deaths, never hidden enemy locations. Humans receive role/objective advice, never movement control.',
    criteria:Object.fromEntries(Object.entries(plans).map(([key,p])=>[key,
      `${p.phase_name}; target ${p.target_site}; attack delay ${p.timing}s; ${Object.entries(p.assignments).map(([id,pos])=>`${id} -> ${pos}`).join('; ')}`]))}}};
}
export function decodeAnswer(state,body){
  const answer=body?.answers?.deployment,plan=plansFor(state)[answer?.choice];
  if(answer?.type!=='choice'||!plan)throw new Error('invalid model answer');
  return {plan,choice:answer.choice,confidence:answer.confidence??null,probabilities:answer.probabilities??null,model:body.model??null,usage:body.usage??null};
}
export function wireReply(state,plan){
  validatePlan(state,plan);
  const targets=state.bots.map(b=>state.positions.findIndex(p=>p.id===plan.assignments[b.id]));
  return `YJ3 ${state.round} ${state.request} ${state.nonce} ${state.roster_revision} ${plan.phase} ${plan.target_site} ${plan.formation} ${plan.timing} ${targets.join(' ')}`;
}
export async function decide(state,{apiKey,fetchFn=fetch,timeout=1800}={}){
  if(!apiKey)throw new Error('missing OpenRouter key');
  const response=await fetchFn('https://openrouter.ai/api/alpha/decisions',{
    method:'POST',headers:{Authorization:`Bearer ${apiKey}`,'Content-Type':'application/json','X-Title':'YaPB Jev Local MVP'},
    body:JSON.stringify(requestFor(state)),signal:AbortSignal.timeout(timeout)});
  if(!response.ok)throw new Error(`OpenRouter HTTP ${response.status}`);
  return decodeAnswer(state,await response.json());
}
