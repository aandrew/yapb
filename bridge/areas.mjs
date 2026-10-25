import {readFileSync} from 'node:fs';
const catalogue=JSON.parse(readFileSync(new URL('./config/de_dust2.areas.json',import.meta.url),'utf8'));
export const areas=catalogue.areas;
const normalise=value=>String(value).toLowerCase().replace(/[^a-z0-9]/g,'');
export function resolveArea(value) {
  const key=normalise(value);
  return areas.find(a=>[a.id,a.label,...a.aliases].some(name=>normalise(name)===key))??null;
}
function inside(polygon,[x,y]) {
  let within=false;
  for(let i=0,j=polygon.length-1;i<polygon.length;j=i++){
    const [xi,yi]=polygon[i],[xj,yj]=polygon[j];
    if((yi>y)!==(yj>y)&&x<(xj-xi)*(y-yi)/(yj-yi)+xi)within=!within;
  }
  return within;
}
export function areaFor(origin) {
  if(!Array.isArray(origin)||origin.length!==3||origin.some(v=>!Number.isFinite(v)))return null;
  const matches=areas.filter(a=>origin[2]>=a.z_range[0]&&origin[2]<=a.z_range[1]&&inside(a.polygon,origin));
  return matches.sort((a,b)=>{
    const size=p=>Math.abs(p.polygon.reduce((s,[x,y],i,poly)=>s+x*poly[(i+1)%poly.length][1]-y*poly[(i+1)%poly.length][0],0));
    return size(a)-size(b);
  })[0]??null;
}
function annotation(origin) {
  const a=areaFor(origin);
  return {area:a?.id??'unmapped',area_label:a?.label??'Unmapped',area_status:a?.status??'unmapped'};
}
export function enrichState(state) {
  if(state.map!==catalogue.map)return state;
  return {...state,area_catalogue_status:catalogue.status,
    bots:state.bots.map(b=>({...b,...(b.connected?annotation(b.origin):{area:'disconnected',area_label:'Disconnected',area_status:'unmapped'})})),
    positions:state.positions.map(p=>({...p,...annotation(p.origin)})),
    observed_contacts:(state.observed_contacts??[]).map(c=>({...c,...annotation(c.origin)}))};
}
