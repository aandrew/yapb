import {areas,resolveArea} from './areas.mjs';
const query=process.argv.slice(2).join(' ');
if(!query){console.log(areas.map(a=>`${a.id}: ${a.label} (${a.waypoint_ids.length} nodes; ${a.status})`).join('\n'));}
else {
  const area=resolveArea(query);
  if(!area){console.error('Unknown area');process.exitCode=1;}
  else console.log(JSON.stringify(area,null,2));
}
