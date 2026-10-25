import {bestAssignment,phases} from './round-policy.mjs';
export function validateTPlan(state,plan){
  const t=state.t_plan;
  if(!t||![6,7,8,9].includes(plan.phase)||!['A','B'].includes(plan.target_site))throw new Error('invalid T call');
  if(t.locked&&(plan.target_site!==t.target_site||plan.timing!==t.attack_delay||plan.phase!==state.round_state.phase_code))throw new Error('T call cannot be rerolled mid-round');
  if(!t.locked&&(plan.phase!==6||!(plan.timing===0||(Number.isInteger(plan.timing)&&plan.timing>=45&&plan.timing<=80))))throw new Error('invalid attack timing');
  const targets=state.bots.map(b=>state.positions.find(p=>p.id===plan.assignments[b.id]));
  if(targets.some(p=>!p)||new Set(targets.map(p=>p.waypoint)).size!==5)throw new Error('invalid T targets');
  const clearing=plan.phase===7||(plan.phase===6&&plan.timing>0);
  const zone=clearing?(plan.target_site==='A'?'X':'Y'):plan.target_site;
  const stage=plan.phase===6?0:t.stage;
  if(plan.formation!==(clearing?stage:0))throw new Error('invalid clearing stage');
  if(targets.some((p,i)=>{
    if(clearing&&state.bots[i].id===t.fake_id)return p.id!==`t_${plan.target_site==='A'?'b':'a'}_1_entry`;
    return p.zone!==zone||(clearing&&p.stage!==stage);
  }))throw new Error('wrong route or attack stage');
  return plan;
}
export function tPlans(state){
  const t=state.t_plan,plans={};
  const add=(key,site,timing,phase,prefer=true)=>{
    const clearing=phase===7||(phase===6&&timing>0),zone=clearing?(site==='A'?'X':'Y'):site;
    const stage=phase===6?0:t.stage;
    const targets=state.positions.filter(p=>p.zone===zone&&(!clearing||p.stage===stage));
    const plan={phase,phase_name:phases[phase],target_site:site,formation:clearing?stage:0,timing,
      assignments:bestAssignment(state,targets,prefer)};
    if(clearing&&t.fake_id&&state.bots.some(b=>b.id===t.fake_id&&!b.human&&b.alive)){
      plan.assignments[t.fake_id]=`t_${site==='A'?'b':'a'}_1_entry`;
    }
    validateTPlan(state,plan);plans[key]=plan;
  };
  if(!t.locked){
    const delay=45+(state.nonce%36);
    for(const site of ['A','B']){add(`rush_${site}`,site,0,6);add(`clear_then_${site}_${delay}s`,site,delay,6);}
  }else{
    const phase=state.round_state.phase_code;
    add('role_order',t.target_site,t.attack_delay,phase);
    add('closest_order',t.target_site,t.attack_delay,phase,false);
    const hold={phase,phase_name:phases[phase],target_site:t.target_site,formation:phase===7?t.stage:0,timing:t.attack_delay,
      assignments:Object.fromEntries(state.bots.map(b=>[b.id,b.position]))};
    try{validateTPlan(state,hold);plans.hold=hold;}catch{}
  }
  return plans;
}
