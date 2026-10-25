export function attackRoute(s){
 const stage=s.t_plan?.stage??0;
 return stage>=6?'mid_b':stage>=3?'short_a':s.t_plan?.target_site==='B'?'tunnels_b':'long_a';
}
export class TOpeningHistory {
 constructor(){this.scope=null;this.rounds=new Map();this.lastScore=0;}
 observe(s){
  const scope=`${s.server_session}:${s.map}`,score=s.economy?.score_known?s.economy.ct_score+s.economy.t_score:null;
  if(scope!==this.scope||(score===0&&this.lastScore>0)){this.rounds.clear();this.scope=scope;}
  if(score!==null)this.lastScore=score;
  if(s.t_plan?.locked&&!this.rounds.has(s.round))this.rounds.set(s.round,{round:s.round,route:attackRoute(s)});
  while(this.rounds.size>8)this.rounds.delete(this.rounds.keys().next().value);
  const previous=[...this.rounds.values()].filter(x=>x.round!==s.round).slice(-5),routes=['long_a','short_a','tunnels_b','mid_b'];
  const counts=Object.fromEntries(routes.map(r=>[r,previous.filter(x=>x.route===r).length]));
  const offset=((score??s.round)%4+4)%4,ordered=[...routes.slice(offset),...routes.slice(0,offset)];
  ordered.sort((a,b)=>counts[a]-counts[b]);
  return {previous,counts,recommended_route:ordered[0],basis:'Own previous opening allocations only; current enemy evidence takes priority.'};
 }
}
