export const phases=['round_start','gather_info','cautious_rotate','committed_rotate','retake','locate_bomb',
  't_call','t_clear','t_execute','t_postplant','t_recover_bomb','guard_bomb','crunch'];
export function assessCT(state) {
  const e=state.round_state;
  if(state.bomb_planted)return {phase:e.planted_site==='A'||e.planted_site==='B'?4:5,site:e.planted_site};
  if(e.dropped_secured)return {phase:11,site:e.dropped_site};
  const a=e.sightings_a>=2||(e.sightings_a>=1&&e.recent_ct_deaths_a>=1);
  const b=e.sightings_b>=2||(e.sightings_b>=1&&e.recent_ct_deaths_b>=1);
  if(e.bomb_recent&&((e.bomb_site==='A'&&a)||(e.bomb_site==='B'&&b)))return {phase:3,site:e.bomb_site};
  if(e.crunch_active)return {phase:12,site:e.crunch_choke};
  if(a||b){
    let site=a?'A':'B';
    if(a&&b){site=e.sightings_b>e.sightings_a?'B':'A';
      if(e.sightings_a===e.sightings_b&&['A','B'].includes(e.previous_target))site=e.previous_target;}
    return {phase:2,site};
  }
  return {phase:e.deployed?1:0,site:'-'};
}
export function canCrunch(state){
 return state.round_state.crunch_available===true&&!state.bomb_planted&&!state.round_state.dropped_secured&&!state.round_state.crunch_used&&
  state.round_state.phase_code===1&&state.round_state.ct_alive===5&&state.round_state.elapsed>=0&&state.round_state.elapsed<=20;
}
export function validateTacticalPlacement(state,plan) {
  const targets=state.bots.map(b=>state.positions.find(p=>p.id===plan.assignments[b.id]));
  if(targets.some(p=>!p)||new Set(targets.map(p=>p.waypoint)).size!==5)throw new Error('invalid or colliding destinations');
  const counts={};targets.forEach(p=>counts[p.zone]=(counts[p.zone]??0)+1);
  const {phase,site}=assessCT(state);
  if(plan.phase===12){
   if(!(canCrunch(state)||(phase===12&&site===plan.target_site)))throw new Error('crunch unavailable');
   const count=targets.filter(p=>p.crunch_group===plan.target_site).length;
   if(![3,4].includes(count)||count!==plan.formation||targets.some((p,i)=>p.crunch_group!==plan.target_site&&p.id!==state.bots[i].position))throw new Error('crunch leaves its bounded area or moves cover');
   return plan;
  }
  if(plan.phase!==phase||plan.target_site!==site)throw new Error('stale or unsupported phase');
  const first=counts.B===1&&counts.M===1&&counts.S===2&&counts.L===1;
  const second=counts.B===1&&counts.M===1&&counts.A===1&&counts.L===2;
  const hold=state.bots.every(b=>b.position===plan.assignments[b.id]);
  const valid=phase===11?counts.G===5:phase===0||phase===1||phase===5?
    (phase!==0&&plan.formation===0&&hold)||(plan.formation===1&&first)||(plan.formation===2&&second):
    phase===2?(counts[site]===3&&counts[site==='A'?'B':'A']===1&&counts.M===1):
    (phase===3||phase===4)&&counts[site]===5;
  if(!valid)throw new Error('placement violates CT round policy');
  return plan;
}

export function bestAssignment(state,targets,preferRoles=true,fixed={}) {
  const bots=[...state.bots].sort((a,b)=>a.id.localeCompare(b.id));
  let best,bestScore=-Infinity;
  function visit(i,used,assignments,score){
    if(i===5){if(score>bestScore){bestScore=score;best={...assignments};}return;}
    const b=bots[i];
    for(const p of targets){if(used.has(p.waypoint))continue;
      if((fixed[b.id]&&fixed[b.id]!==p.id)||Object.entries(fixed).some(([id,target])=>id!==b.id&&target===p.id))continue;
      const fit=p.preferred_roles.split(',').includes(b.role)?(preferRoles?12:2):0;
      const distance=b.alive?Math.hypot(...b.origin.map((v,k)=>v-p.origin[k]))/1500:0;
      const retain=b.position===p.id?3:0;
      used.add(p.waypoint);assignments[b.id]=p.id;
      visit(i+1,used,assignments,score+fit+retain-distance*(preferRoles?1:4));used.delete(p.waypoint);
    }
  }
  visit(0,new Set(),{},0);
  if(!best)throw new Error('insufficient unique positions');
  return best;
}
function take(state,zone,count,offset=0){
  const positions=state.positions.filter(p=>p.zone===zone);
  const rotated=[...positions.slice(offset),...positions.slice(0,offset)];
  if(rotated.length<count)throw new Error('incomplete tactical registry');
  return rotated.slice(0,count);
}
export function ctPlans(state) {
  const {phase,site}=assessCT(state),plans={};
  if(phase===12){
   const plan={phase,phase_name:phases[12],target_site:site,formation:state.round_state.formation,timing:0,assignments:Object.fromEntries(state.bots.map(b=>[b.id,b.position]))};
   validateTacticalPlacement(state,plan);return {hold_crunch:plan};
  }
  const add=(key,targets,formation=0,preferRoles=true)=>{
    const plan={phase,phase_name:phases[phase],target_site:site,formation,timing:0,
      assignments:bestAssignment(state,targets,preferRoles)};
    validateTacticalPlacement(state,plan);plans[key]=plan;
  };
  const hold={phase,phase_name:phases[phase],target_site:site,formation:0,timing:0,
    assignments:Object.fromEntries(state.bots.map(b=>[b.id,b.position]))};
  try{validateTacticalPlacement(state,hold);plans.hold=hold;}catch{}
  if([0,1,5].includes(phase)){
    add('b_mid_two_short_one_long',[...take(state,'B',1),...take(state,'M',1),...take(state,'S',2),...take(state,'L',1)],1);
    add('b_mid_a_two_long',[...take(state,'B',1),...take(state,'M',1),...take(state,'A',1),...take(state,'L',2)],2);
  }else if(phase===11){
    add('guard_dropped_bomb',take(state,'G',5));
  }else if(phase===2){
    for(const offset of [0,2])add(`cautious_${site}_${offset}`,[...take(state,site,3,offset),...take(state,site==='A'?'B':'A',1),...take(state,'M',1)]);
  }else{
    add(`${phases[phase]}_${site}_roles`,take(state,site,5));
    add(`${phases[phase]}_${site}_nearest`,take(state,site,5),0,false);
  }
  if(canCrunch(state)){
   for(const [choke,label]of [['L','long'],['S','short'],['M','mid'],['B','b_tunnels']]){
    const group=state.positions.filter(p=>p.crunch_group===choke);if(group.length!==4)continue;
    const center=group[0].origin;
    const members=[...state.bots].sort((a,b)=>Math.hypot(...a.origin.map((v,i)=>v-center[i]))-Math.hypot(...b.origin.map((v,i)=>v-center[i])));
    for(const count of [3,4]){
     const moving=new Set(members.slice(0,count).map(b=>b.id));
     const fixed=Object.fromEntries(state.bots.filter(b=>!moving.has(b.id)).map(b=>[b.id,b.position]));
     const targets=[...group.slice(0,count),...Object.values(fixed).map(id=>state.positions.find(p=>p.id===id))];
     try{
      const plan={phase:12,phase_name:phases[12],target_site:choke,formation:count,timing:0,assignments:bestAssignment(state,targets,true,fixed)};
      validateTacticalPlacement(state,plan);plans[`crunch_${label}_${count}`]=plan;
     }catch{}
    }
   }
  }
  return plans;
}
