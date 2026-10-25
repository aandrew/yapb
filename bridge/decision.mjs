import {roles} from './config.mjs';
const token=/^[a-z][a-z0-9_]{0,30}$/;
const vector=x=>Array.isArray(x)&&x.length===3&&x.every(v=>Number.isFinite(v)&&Math.abs(v)<100000);
export function validateState(state) {
  if(state?.version!==2||!token.test(state.map)||!['round_start','poll','telemetry'].includes(state.event)||
    ['round','request','nonce','roster_revision'].some(k=>!Number.isSafeInteger(state[k])||state[k]<0)||
    !Number.isFinite(state.game_time)||!Array.isArray(state.bots)||state.bots.length!==5||
    !Array.isArray(state.positions)||state.positions.length<5||state.positions.length>32)throw new Error('invalid state');
  if(new Set(state.bots.map(b=>b.id)).size!==5||state.bots.some(b=>!token.test(b.id)||!roles.includes(b.role)||
    !Number.isInteger(b.slot)||b.slot<0||b.slot>32||!Number.isInteger(b.userid)||b.userid<0||
    !Number.isInteger(b.generation)||b.generation<0||typeof b.alive!=='boolean'||typeof b.connected!=='boolean'||
    !vector(b.origin)||!Number.isFinite(b.health)))throw new Error('invalid roster');
  const connected=state.bots.filter(b=>b.connected);
  if(connected.some(b=>b.slot===0)||new Set(connected.map(b=>b.slot)).size!==connected.length)throw new Error('duplicate game binding');
  if(new Set(state.positions.map(p=>p.id)).size!==state.positions.length||state.positions.some(p=>!token.test(p.id)||
    !['A','B'].includes(p.site)||!vector(p.origin)||!vector(p.look_at)||!Number.isInteger(p.waypoint)||p.waypoint<0||
    !Number.isFinite(p.radius)||p.radius<8||p.radius>96||!['stand','crouch'].includes(p.stance)||
    typeof p.preferred_roles!=='string'||p.preferred_roles.split(',').some(r=>!roles.includes(r))))throw new Error('invalid positions');
  if(state.positions.filter(p=>p.site==='A').length<3||state.positions.filter(p=>p.site==='B').length<2)throw new Error('incomplete position registry');
  return state;
}

function assignGroup(bots,positions) {
  // Find a unique placement for each member, preferring role fit and retaining
  // existing positions. Bound search to the best six candidates per site.
  const candidates=positions.map(p=>({p,score:bots.reduce((s,b)=>s+(p.preferred_roles.split(',').includes(b.role)?1:0),0)}))
    .sort((a,b)=>b.score-a.score||a.p.id.localeCompare(b.p.id)).slice(0,6).map(x=>x.p);
  let best, bestScore=-Infinity;
  function visit(i,used,result,score) {
    if(i===bots.length){if(score>bestScore){bestScore=score;best={...result};}return;}
    const b=bots[i];
    for(const p of candidates){
      if(used.has(p.id))continue;
      const fit=p.preferred_roles.split(',').includes(b.role)?10:0;
      const travel=b.alive?Math.hypot(...b.origin.map((v,k)=>v-p.origin[k]))/4000:0;
      const retain=b.position===p.id?5:0;
      used.add(p.id);result[b.id]=p.id;
      visit(i+1,used,result,score+fit+retain-travel);
      used.delete(p.id);
    }
  }
  visit(0,new Set(),{},0);
  return best;
}

export function validatePlan(state,plan) {
  if(!plan||!plan.assignments||Object.keys(plan.assignments).length!==5)throw new Error('invalid plan');
  const targets=state.bots.map(b=>state.positions.find(p=>p.id===plan.assignments[b.id]));
  if(targets.some(p=>!p)||new Set(targets.map(p=>p.id)).size!==5||
    targets.filter(p=>p.site==='A').length!==3||targets.filter(p=>p.site==='B').length!==2)throw new Error('invalid split or target');
  return plan;
}

export function plansFor(state) {
  validateState(state);
  const bots=[...state.bots].sort((a,b)=>a.id.localeCompare(b.id));
  const plans={};
  if(bots.every(b=>state.positions.some(p=>p.id===b.position))){
    const hold={assignments:Object.fromEntries(bots.map(b=>[b.id,b.position]))};
    try{validatePlan(state,hold);plans.hold=hold;}catch{}
  }
  for(let i=0;i<3;i++)for(let j=i+1;j<4;j++)for(let k=j+1;k<5;k++){
    const a=[bots[i],bots[j],bots[k]], ids=new Set(a.map(b=>b.id));
    const b=bots.filter(bot=>!ids.has(bot.id));
    const plan={assignments:{...assignGroup(a,state.positions.filter(p=>p.site==='A')),...assignGroup(b,state.positions.filter(p=>p.site==='B'))}};
    validatePlan(state,plan);
    plans[`a_${a.map(bot=>bot.id).join('_')}`]=plan;
  }
  return plans;
}

export function requestFor(state,model='typesafe/jev-1.14') {
  const plans=plansFor(state);
  return {model,state,questions:{deployment:{type:'choice',
    instructions:'Choose one complete CT team placement from the supplied candidates. Exactly three stable identities are assigned A and two B. Respect persistent roles and observed game state. Keep current orders with hold when appropriate; change them only when the updated state warrants movement. Avoid unnecessary cross-map travel and do not use hidden enemy knowledge. Dead or disconnected identities retain assignments but cannot move. Each named position includes its destination, viewing target and preferred roles. This is a fresh decision every 10 seconds, not a request for free-form tactics.',
    criteria:Object.fromEntries(Object.entries(plans).map(([key,p])=>[key,
      `${key==='hold'?'Keep current orders. ':''}${Object.entries(p.assignments).map(([id,pos])=>`${id} -> ${pos}`).join('; ')}`]))
  }}};
}

export function decodeAnswer(state,body) {
  const answer=body?.answers?.deployment, plan=plansFor(state)[answer?.choice];
  if(answer?.type!=='choice'||!plan)throw new Error('invalid model answer');
  return {plan,choice:answer.choice,confidence:answer.confidence??null,probabilities:answer.probabilities??null,model:body.model??null,usage:body.usage??null};
}

export function wireReply(state,plan) {
  validatePlan(state,plan);
  const targets=state.bots.map(b=>state.positions.findIndex(p=>p.id===plan.assignments[b.id]));
  return `YJ2 ${state.round} ${state.request} ${state.nonce} ${state.roster_revision} ${targets.join(' ')}`;
}

export async function decide(state,{apiKey,fetchFn=fetch,timeout=1800}={}) {
  if(!apiKey)throw new Error('missing OpenRouter key');
  const response=await fetchFn('https://openrouter.ai/api/alpha/decisions',{
    method:'POST',headers:{Authorization:`Bearer ${apiKey}`,'Content-Type':'application/json','X-Title':'YaPB Jev Local MVP'},
    body:JSON.stringify(requestFor(state)),signal:AbortSignal.timeout(timeout)
  });
  if(!response.ok)throw new Error(`OpenRouter HTTP ${response.status}`);
  return decodeAnswer(state,await response.json());
}
