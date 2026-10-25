import {DecisionStore,validateAppliedOrder} from './decision-store.mjs';
import dgram from 'node:dgram';
import http from 'node:http';
import {appendFileSync, mkdirSync, writeFileSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
import {dirname, join} from 'node:path';
import {decide, validateState, wireReply} from './decision.mjs';
import {enrichState} from './areas.mjs';
import {SilentOrchestrator} from './orchestrator.mjs';
const orchestrator=new SilentOrchestrator();

const root = dirname(fileURLToPath(import.meta.url));
mkdirSync(join(root,'logs'), {recursive:true});
const database=new DecisionStore(join(root,'logs','decisions.sqlite'));
let databaseError=null;
const socket = dgram.createSocket('udp4');
const seen = new Map();
const busy = new Set();
function record(event) {
  const row = {time:new Date().toISOString(), ...event};
  appendFileSync(join(root,'logs','decisions.jsonl'), JSON.stringify(row)+'\n');
  try {database.record(row);databaseError=null;} catch {databaseError='database_write_failed';console.error(databaseError);}
  console.log(JSON.stringify(row));
}
socket.on('message', async (buffer, peer) => {
  if (peer.address !== '127.0.0.1' || buffer.length > 60000) return;
  let state;
  try {
    const packet=JSON.parse(buffer);
    if(packet.event==='order_applied'){record({event:'order_applied',order:validateAppliedOrder(packet)});return;}
    if(packet.event==='world_state'){
      orchestrator.ingestWorld(packet);writeFileSync(join(root,'logs','world-state.json'),JSON.stringify(orchestrator.snapshot()));return;
    }
    if(packet.event==='combat_report'){
      if(orchestrator.ingestCombat(packet))record({event:'combat_report',report:packet});return;
    }
    state=orchestrator.callerView(enrichState(validateState(packet)));
  } catch { return; }
  if (state.event === 'telemetry') { record({event:'telemetry',state}); return; }
  const key = `${peer.port}:${state.round}:${state.request}:${state.nonce}:${state.roster_revision}`;
  if (seen.has(key)) {
    const reply=seen.get(key);
    if (reply) socket.send(reply, peer.port, peer.address);
    return;
  }
  if (busy.has(peer.port) || busy.size >= 2) return;
  seen.set(key,null);
  if (seen.size > 128) seen.delete(seen.keys().next().value);
  busy.add(peer.port);
  const start=performance.now();
  record({event:'request',state});
  try {
    const result = await decide(state, {apiKey:process.env.OPENROUTER_API_KEY,onRequest:request=>record({event:'model_request',server_session:state.server_session,map:state.map,nonce:state.nonce,roster_revision:state.roster_revision,team:state.team,round:state.round,request:state.request,payload:request})});
    const reply=Buffer.from(wireReply(state,result.plan));
    seen.set(key,reply);
    socket.send(reply, peer.port, peer.address);
    record({event:'decision',server_session:state.server_session,map:state.map,team:state.team,round:state.round,request:state.request,nonce:state.nonce,roster_revision:state.roster_revision,state_event:state.event,game_time:state.game_time,latency_ms:Math.round(performance.now()-start),...result});
  } catch (error) {
    // Never log request headers, credentials, or raw provider errors.
    record({event:'fallback',server_session:state.server_session,map:state.map,team:state.team,roster_revision:state.roster_revision,round:state.round,request:state.request,nonce:state.nonce,error:error.message});
  } finally { busy.delete(peer.port); }
});
socket.on('error', error => { console.error(error.code); socket.close(); process.exitCode=1; });
socket.bind(28777,'127.0.0.1',()=>console.log('YaPB Jev bridge listening on 127.0.0.1:28777'));

const diagnostics=http.createServer((req,res)=>{
 if(req.method!=='GET'){res.writeHead(405);res.end();return;}
 const path=req.url?.split('?')[0];
 const params=new URL(req.url,'http://localhost').searchParams;
 const detail=path?.match(/^\/decisions\/(\d+)$/);
 const result=path==='/decisions'?database.list({team:params.get('team'),round:params.has('round')?Number(params.get('round')):undefined,limit:params.get('limit')??50}):detail?database.detail(Number(detail[1])):path==='/health'?{...orchestrator.health(),database:{...database.stats(),error:databaseError}}:path==='/world'||path==='/state'?orchestrator.snapshot():path==='/callers/CT'?orchestrator.caller('CT'):path==='/callers/T'?orchestrator.caller('T'):undefined;
 res.writeHead(result===undefined?404:result===null?503:200,{'Content-Type':'application/json','Cache-Control':'no-store'});
 res.end(JSON.stringify(result??{error:'not available'}));
});
diagnostics.on('error',error=>{console.error(error.code);socket.close();process.exitCode=1;});
diagnostics.listen(28778,'127.0.0.1',()=>console.log('Silent orchestrator diagnostics on 127.0.0.1:28778'));
