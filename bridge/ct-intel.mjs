import {areaFor} from './areas.mjs';
const groups={long:new Set(['long','long_corner','long_platform','pit','side_pit','long_box','long_doors','outside_long']),short:new Set(['short_a','short_stairs','cat','catwalk','cat_boost']),mid:new Set(['mid_doors','middle','top_mid','xbox','suicide']),b_tunnels:new Set(['upper_tunnels','lower_tunnels','tunnel_stairs'])};
export class CTTendencies {
 constructor(){this.scope=null;this.rounds=new Map();this.lastScore=0;}
 observe(s){
  const scope=`${s.server_session}:${s.map}`,score=s.economy?.score_known?s.economy.ct_score+s.economy.t_score:null;
  if(scope!==this.scope||(score===0&&this.lastScore>0)){this.rounds.clear();this.scope=scope;}
  if(score!==null)this.lastScore=score;
  if(!this.rounds.has(s.round))this.rounds.set(s.round,{round:s.round,peaks:{},early:{}});
  const current=this.rounds.get(s.round),counts={};
  for(const c of s.observed_contacts??[]){
   const age=c.age??s.game_time-c.time;if(!Number.isFinite(age)||age<0||age>8)continue;
   const area=areaFor(c.origin)?.id;
   for(const [name,areas] of Object.entries(groups))if(areas.has(area)){(counts[name]??=new Set()).add(c.enemy_id);}
  }
  for(const [name,ids] of Object.entries(counts)){current.peaks[name]=Math.max(current.peaks[name]??0,ids.size);if(s.round_state.elapsed>=0&&s.round_state.elapsed<=35)current.early[name]=Math.max(current.early[name]??0,ids.size);}
  while(this.rounds.size>7)this.rounds.delete(this.rounds.keys().next().value);
  const previous=[...this.rounds.values()].filter(r=>r.round!==s.round).slice(-5);
  return {source:'CT team sightings only',observed_previous_rounds:previous.length,window_rounds:previous.map(r=>r.round),approaches:Object.fromEntries(Object.keys(groups).map(name=>{
   const early=previous.filter(r=>(r.early[name]??0)>=3).length,total=previous.filter(r=>(r.peaks[name]??0)>=3).length;
   return [name,{three_plus_rounds:total,early_three_plus_rounds:early,stack_recommended:early>=2&&early/previous.length>=0.5}];
  }))};
 }
}
export function adaptiveCTOptions(s){
 if(s.team!=='CT'||s.bomb_planted||s.round_state.dropped_secured)return {};
 const e=s.round_state,out={};
 if([0,1].includes(e.phase_code)&&s.bots.filter(b=>b.alive&&b.connected).length===5&&e.elapsed<=25){
  for(const [name,code] of [['long','L'],['short','S'],['mid','M'],['b_tunnels','B']])if(s.ct_tendencies?.approaches?.[name]?.stack_recommended)out[`tendency_stack_${name}_4`]={group:code,formation:7};
 }
 if(e.phase_code===1){if(e.sightings_a>=1||e.heard_a>=1)out.preempt_reinforce_A={site:'A',formation:8};if(e.sightings_b>=1||e.heard_b>=1)out.preempt_reinforce_B={site:'B',formation:9};}
 return out;
}
