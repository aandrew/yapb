import {rcon} from './control.mjs';
for(const command of [
  'yb_quota 0', 'yb kickall', 'mp_autoteambalance 0', 'mp_limitteams 0',
  'yb_systemone 1', 'yb_chat 1', 'yb_radio_mode 0',
  ...[1,2,3,4,5].map(i=>`yb add 2 0 2 1 CT_Jev_${i}`),
  'yb add 0 2 1 1 T_Practice',
]) {
  const result=await rcon(command);
  console.log(command, result.trim());
}
console.log(await rcon('sv_restart 1'));
