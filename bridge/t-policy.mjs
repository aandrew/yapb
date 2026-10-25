import {bestAssignment,phases} from './round-policy.mjs';
export function adaptation(state){
  const phase=state.round_state.phase_code,now=state.game_time;
  const recent=(state.round_events??[]).filter(e=>e.type==='death'&&Number.isFinite(e.time)&&now-e.time>=0&&now-e.time<=12);
  const picks=recent.filter(e=>e.victim_team==='C'&&e.killer_team==='T');
  const own=recent.filter(e=>e.victim_team==='T').length;
  const live=state.t_plan.elapsed>=0,t=state.round_state.t_alive;
  const same=picks.some(e=>e.observed_site===state.t_plan.target_site);
  const other=picks.some(e=>e.observed_site===(state.t_plan.target_site==='A'?'B':'A'));
  const available=!state.bomb_planted&&!state.bomb_objective?.dropped;
  const opportunity=available&&!state.t_plan.retargeted&&[7,8].includes(phase)&&t>=2&&other;
  return {early:available&&phase===7&&live&&t>0&&same,opportunity,
    casualties:available&&!state.t_plan.retargeted&&[7,8].includes(phase)&&own>=2&&t>=2,
    pivot:opportunity||(available&&!state.t_plan.retargeted&&[7,8].includes(phase)&&own>=2&&t>=2)};
}
export function validateTPlan(state,plan){
  const t=state.t_plan;
  if(!t||![6,7,8,9,10].includes(plan.phase)||!['A','B'].includes(plan.target_site))throw new Error('invalid T call');
  const adapt=adaptation(state),pivot=t.locked&&plan.target_site!==t.target_site;
  const replan=state.round_state.phase_code===13&&t.replanning===true&&[7,8].includes(plan.phase);
  if(t.locked&&(plan.timing!==t.attack_delay||
    (!replan&&plan.phase!==state.round_state.phase_code&&!((pivot?adapt.opportunity:adapt.early)&&plan.phase===8))||
    (!replan&&pivot&&!adapt.pivot)))throw new Error('T call cannot be rerolled without current casualty evidence');
  if(!t.locked&&plan.timing===0&&t.rush_allowed===false)throw new Error('opening rush not available this round');
  if(!t.locked&&(plan.phase!==6||!(plan.timing===0||(Number.isInteger(plan.timing)&&plan.timing>=30&&plan.timing<=60))))throw new Error('invalid attack timing');
  const targets=state.bots.map(b=>state.positions.find(p=>p.id===plan.assignments[b.id]));
  if(targets.some(p=>!p)||new Set(targets.map(p=>p.waypoint)).size!==5)throw new Error('invalid T targets');
  const clearing=plan.phase===7||(plan.phase===6&&plan.timing>0);
  const zone=plan.phase===10?'R':clearing?(plan.target_site==='A'?'X':'Y'):plan.target_site;
  const stage=plan.phase===6?(clearing?plan.formation:0):(pivot||replan?2:t.stage);
  if(plan.phase===6&&clearing&&!(stage===0||(stage===3&&plan.target_site==='A')||(stage===6&&plan.target_site==='B')))throw Error('invalid opening route');
  if(plan.formation!==(clearing?stage:0))throw new Error('invalid clearing stage');
  if(plan.phase===10&&(!state.bomb_objective?.dropped||plan.assignments[state.bomb_objective.collector_id]!==state.bomb_objective.pickup_position))throw new Error('bomb recovery requires assigned collector');
  if(clearing&&state.bots.some((b,i)=>b.carrying_c4&&targets[i].id!==`t_${plan.target_site.toLowerCase()}_${stage}_support`))throw new Error('C4 must stay on the main support route');
  if(targets.some((p,i)=>{
    if(clearing&&!state.bots[i].carrying_c4&&state.bots[i].id===t.fake_id)return p.id!==`t_${plan.target_site==='A'?'b':'a'}_1_entry`;
    return p.zone!==zone||(clearing&&p.stage!==stage);
  }))throw new Error('wrong route or attack stage');
  return plan;
}
export function tPlans(state){
  const t=state.t_plan,plans={};
  if(state.round_state.phase_code===10){
    const o=state.bomb_objective;
    const plan={phase:10,phase_name:phases[10],target_site:t.target_site,formation:0,timing:t.attack_delay,
      assignments:bestAssignment(state,state.positions.filter(p=>p.zone==='R'),true,{[o.collector_id]:o.pickup_position})};
    validateTPlan(state,plan);return {recover_bomb_first:plan};
  }
  const add=(key,site,timing,phase,prefer=true,openingStage=0)=>{
    const pivot=t.locked&&site!==t.target_site;
    const clearing=phase===7||(phase===6&&timing>0),zone=clearing?(site==='A'?'X':'Y'):site;
    const stage=phase===6?openingStage:(pivot||state.round_state.phase_code===13?2:t.stage);
    const targets=state.positions.filter(p=>p.zone===zone&&(!clearing||p.stage===stage));
    const carrier=state.bots.find(b=>b.carrying_c4);
    const fixed=clearing&&carrier?{[carrier.id]:`t_${site.toLowerCase()}_${stage}_support`}:{};
    const plan={phase,phase_name:phases[phase],target_site:site,formation:clearing?stage:0,timing,
      assignments:bestAssignment(state,targets,prefer,fixed)};
    if(clearing&&t.fake_id&&state.bots.some(b=>b.id===t.fake_id&&!b.carrying_c4&&!b.human&&b.alive)){
      plan.assignments[t.fake_id]=`t_${site==='A'?'b':'a'}_1_entry`;
    }
    validateTPlan(state,plan);plans[key]=plan;
  };
  if(state.round_state.phase_code===13){
    const phase=t.attack_at-state.game_time>8?7:8;
    for(const site of ['A','B'])add(`recover_then_attack_${site}`,site,t.attack_delay,phase);
    return plans;
  }
  if(!t.locked){
    const delay=30+(state.nonce%31);
    for(const site of ['A','B']){if(t.rush_allowed!==false)add(`rush_${site}`,site,0,6);add(`clear_then_${site}_${delay}s`,site,delay,6);}
    add(`short_to_A_${delay}s`,'A',delay,6,true,3);
    add(`mid_to_B_${delay}s`,'B',delay,6,true,6);
  }else{
    const phase=state.round_state.phase_code;
    add('role_order',t.target_site,t.attack_delay,phase);
    add('closest_order',t.target_site,t.attack_delay,phase,false);
    const hold={phase,phase_name:phases[phase],target_site:t.target_site,formation:phase===7?t.stage:0,timing:t.attack_delay,
      assignments:Object.fromEntries(state.bots.map(b=>[b.id,b.position]))};
    try{validateTPlan(state,hold);plans.hold=hold;}catch{}
    const adapt=adaptation(state);
    if(adapt.early)add('site_pick_execute_now',t.target_site,t.attack_delay,8);
    if(adapt.opportunity)add('other_site_pick_execute',t.target_site==='A'?'B':'A',t.attack_delay,8);
    if(adapt.casualties&&(phase===8||t.attack_at-state.game_time>=8))add('casualties_switch_site',t.target_site==='A'?'B':'A',t.attack_delay,phase);
  }
  return plans;
}
