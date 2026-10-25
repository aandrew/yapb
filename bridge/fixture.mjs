import {loadConfig} from './config.mjs';
export function fixture(team='CT') {
  const {roster,map}=loadConfig(),members=team==='T'?roster.terrorists:roster.bots;
  return {version:3,team,event:'round_start',map:map.map,round:1,request:7,nonce:123,roster_revision:1,game_time:1,
    time_left:180,poll_interval:5,bomb_planted:false,observed_contacts:[],round_events:[],
    round_state:{phase:team==='T'?'t_call':'round_start',phase_code:team==='T'?6:0,target_site:'-',deployed:false,
      sightings_a:0,sightings_b:0,recent_ct_deaths_a:0,recent_ct_deaths_b:0,bomb_recent:false,bomb_site:'-',planted_site:'-',previous_target:'-'},
    t_plan:{locked:false,target_site:'-',attack_delay:-1,attack_at:0,elapsed:-7,stage:0},
    positions:map.positions.map((p,i)=>({...p,preferred_roles:p.preferred_roles.join(','),waypoint:p.source_waypoint??i+10})),
    bots:members.map((b,i)=>({id:b.id,role:b.role,caller:b.caller,human:false,slot:i+1,userid:i+101,generation:i+1,
      connected:true,alive:true,health:100,money:4000,weapon:0,origin:[i*60,2200,0],position:'pending',site:'-'}))};
}
