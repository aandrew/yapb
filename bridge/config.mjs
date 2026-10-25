import {readFileSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
export const roles = ['awper','lurker','entry','support'];
const token = /^[a-z][a-z0-9_]{0,30}$/;
const vector = x => Array.isArray(x)&&x.length===3&&x.every(v=>Number.isFinite(v)&&Math.abs(v)<100000);
export function validateConfig(roster,map) {
  if(!Array.isArray(roster?.bots)||roster.bots.length!==5||!Array.isArray(roster.terrorists)||roster.terrorists.length!==5)throw new Error('Configuration needs five identities per team');
  if(new Set(roster.bots.map(b=>b.id)).size!==5||new Set(roster.bots.map(b=>b.name)).size!==5)throw new Error('Duplicate identity or bot name');
  const all=[...roster.bots,...roster.terrorists];
  if(new Set(all.map(b=>b.id)).size!==10||new Set(all.map(b=>b.name)).size!==10)throw new Error('Duplicate cross-team identity');
  for(const team of [roster.bots,roster.terrorists])if(team.filter(b=>b.caller).length!==1)throw new Error('Each team needs one preferred caller');
  for(const b of all){
    if(!token.test(b.id)||!/^\w{1,31}$/.test(b.name)||!roles.includes(b.role)||
      !Number.isInteger(b.difficulty)||b.difficulty<0||b.difficulty>4||
      !Number.isInteger(b.personality)||b.personality<0||b.personality>2)throw new Error('Invalid roster profile');
  }
  if(!token.test(map?.map)||!Array.isArray(map.positions)||map.positions.length<15||map.positions.length>64)throw new Error('Invalid position registry');
  if(new Set(map.positions.map(p=>p.id)).size!==map.positions.length)throw new Error('Duplicate position');
  for(const p of map.positions){
    if(!token.test(p.id)||!['A','B','M'].includes(p.site)||!['A','B','M','S','L','X','Y'].includes(p.zone)||!Number.isInteger(p.stage)||!vector(p.origin)||!vector(p.look_at)||
      !Number.isFinite(p.radius)||p.radius<8||p.radius>96||!['stand','crouch'].includes(p.stance)||
      !Array.isArray(p.preferred_roles)||!p.preferred_roles.length||p.preferred_roles.some(r=>!roles.includes(r)))throw new Error('Invalid named position');
  }
  for(const [zone,count]of Object.entries({A:5,B:5,M:1,S:2,L:2}))if(map.positions.filter(p=>p.zone===zone).length<count)throw new Error('Incomplete CT formation registry');
  for(const zone of ['X','Y'])for(const stage of [0,1,2])if(map.positions.filter(p=>p.zone===zone&&p.stage===stage).length!==5)throw new Error('Incomplete T clearing route');
  return {roster,map};
}
export function loadConfig() {
  return validateConfig(
    JSON.parse(readFileSync(fileURLToPath(new URL('./config/roster.json',import.meta.url)),'utf8')),
    JSON.parse(readFileSync(fileURLToPath(new URL('./config/de_dust2.positions.json',import.meta.url)),'utf8')));
}
export function engineConfig(roster,map,team='CT') {
  validateConfig(roster,map);
  return [`map ${map.map}`,
    ...(team==='T'?roster.terrorists:roster.bots).map(b=>`bot ${b.id} ${b.name} ${b.role} ${b.difficulty} ${b.personality} ${b.caller?1:0}`),
    ...map.positions.map(p=>`position ${p.id} ${p.site} ${p.zone} ${p.stage} ${p.origin.join(' ')} ${p.look_at.join(' ')} ${p.radius} ${p.stance} ${p.preferred_roles.join(',')}`)
  ].join('\n')+'\n';
}
