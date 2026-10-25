import {readFileSync} from 'node:fs';
const safe=/^[a-z][a-z0-9_]{0,30}$/;
export function loadStrategies(map,team){
 if(!safe.test(map)||!['T','CT'].includes(team))throw Error('invalid strategy location');
 try{
  const config=JSON.parse(readFileSync(new URL(`../strategy/${map}/${team}/config.json`,import.meta.url),'utf8'));
  if(config.version!==1||!Array.isArray(config.strategies)||config.strategies.length>32)throw Error('invalid strategy config');
  const ids=new Set();
  for(const s of config.strategies){
   if(!safe.test(s.id)||ids.has(s.id)||!['pistol','other','any'].includes(s.rounds)||typeof s.name!=='string'||typeof s.when!=='string'||!Array.isArray(s.plan_prefixes)||!s.plan_prefixes.length||s.plan_prefixes.some(p=>!/^[a-zA-Z][a-zA-Z0-9_]{0,63}$/.test(p))||s.name.length>120||s.when.length>1200)throw Error('invalid strategy entry');
   ids.add(s.id);
  }
  return config;
 }catch(error){if(error.code==='ENOENT')return {version:1,strategies:[]};throw error;}
}
export function strategyContext(state,plans){
 const config=loadStrategies(state.map,state.team),pistol=state.t_plan?.pistol_round;
 return {map:state.map,team:state.team,pistol_round:pistol??null,
  selection_rule:'Choose a supplied deployment matching a suitable strategy. Conditions are guidance, not claims about enemy locations. Active bomb objectives override opening strategies.',
  strategies:config.strategies.filter(s=>s.rounds==='any'||pistol===undefined||(s.rounds==='pistol')===pistol).map(s=>({...s,choices:Object.keys(plans).filter(k=>s.plan_prefixes.some(p=>k.startsWith(p)))})).filter(s=>s.choices.length)};
}
