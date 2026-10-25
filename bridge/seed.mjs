import {rcon} from './control.mjs';
import {loadConfig} from './config.mjs';
const {roster}=loadConfig();
for(const command of [
  'yb_quota 0', 'yb kickall', 'mp_autoteambalance 0', 'mp_limitteams 0',
  'yb_systemone 1', 'yb_systemone_poll_interval 10', 'yb_chat 1', 'yb_radio_mode 0',
  ...roster.bots.map(b=>`yb add ${b.difficulty} ${b.personality} 2 1 ${b.name}`),
  'yb add 0 2 1 1 T_Practice',
]) {
  const result=await rcon(command);
  console.log(command, result.trim());
}
console.log(await rcon('sv_restart 1'));
