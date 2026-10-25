import {readFileSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
export const roles = ['awper','lurker','entry','support'];
const token = /^[a-z][a-z0-9_]{0,30}$/;
const vector = x => Array.isArray(x)&&x.length===3&&x.every(v=>Number.isFinite(v)&&Math.abs(v)<100000);
export function validateConfig(roster,map) {
  if(!Array.isArray(roster?.bots)||roster.bots.length!==5)throw new Error('Configuration needs five identities');
  if(new Set(roster.bots.map(b=>b.id)).size!==5||new Set(roster.bots.map(b=>b.name)).size!==5)throw new Error('Duplicate identity or bot name');
  for(const b of roster.bots){
    if(!token.test(b.id)||!/^\w{1,31}$/.test(b.name)||!roles.includes(b.role)||
      !Number.isInteger(b.difficulty)||b.difficulty<0||b.difficulty>4||
      !Number.isInteger(b.personality)||b.personality<0||b.personality>2)throw new Error('Invalid roster profile');
  }
  if(!token.test(map?.map)||!Array.isArray(map.positions)||map.positions.length<5||map.positions.length>32)throw new Error('Invalid position registry');
  if(new Set(map.positions.map(p=>p.id)).size!==map.positions.length)throw new Error('Duplicate position');
  for(const p of map.positions){
    if(!token.test(p.id)||!['A','B'].includes(p.site)||!vector(p.origin)||!vector(p.look_at)||
      !Number.isFinite(p.radius)||p.radius<8||p.radius>96||!['stand','crouch'].includes(p.stance)||
      !Array.isArray(p.preferred_roles)||!p.preferred_roles.length||p.preferred_roles.some(r=>!roles.includes(r)))throw new Error('Invalid named position');
  }
  if(map.positions.filter(p=>p.site==='A').length<3||map.positions.filter(p=>p.site==='B').length<2)throw new Error('Position registry cannot support 3 A / 2 B');
  return {roster,map};
}
export function loadConfig() {
  return validateConfig(
    JSON.parse(readFileSync(fileURLToPath(new URL('./config/roster.json',import.meta.url)),'utf8')),
    JSON.parse(readFileSync(fileURLToPath(new URL('./config/de_dust2.positions.json',import.meta.url)),'utf8')));
}
export function engineConfig(roster,map) {
  validateConfig(roster,map);
  return [`map ${map.map}`,
    ...roster.bots.map(b=>`bot ${b.id} ${b.name} ${b.role}`),
    ...map.positions.map(p=>`position ${p.id} ${p.site} ${p.origin.join(' ')} ${p.look_at.join(' ')} ${p.radius} ${p.stance} ${p.preferred_roles.join(',')}`)
  ].join('\n')+'\n';
}
