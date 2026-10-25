import {rcon} from './control.mjs';
// Directors create the named roster and replace bots with humans as needed.
for(const command of ['yb_systemone 0','yb_quota 0','yb kickall','mp_autoteambalance 0','mp_limitteams 0',
  'yb_chat 1','yb_radio_mode 0','yb_systemone_poll_interval 10','yb_systemone 1']) {
  console.log(command,(await rcon(command)).trim());
}
console.log(await rcon('sv_restart 1'));
