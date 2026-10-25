import {loadConfig} from './config.mjs';
export function fixture() {
  const {roster,map}=loadConfig();
  return {version:2,event:'round_start',map:map.map,round:1,request:7,nonce:123,roster_revision:1,game_time:1,
    time_left:180,poll_interval:10,bomb_planted:false,observed_contacts:[],
    positions:map.positions.map((p,i)=>({...p,preferred_roles:p.preferred_roles.join(','),waypoint:i+10})),
    bots:roster.bots.map((b,i)=>({id:b.id,role:b.role,slot:i+1,userid:i+101,generation:i+1,
      connected:true,alive:true,health:100,money:4000,weapon:0,origin:[i*60,2200,0],position:'pending',site:'-'}))};
}
