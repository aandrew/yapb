import dgram from 'node:dgram';
import {appendFileSync, mkdirSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
import {dirname, join} from 'node:path';
import {decide, validateState, wireReply} from './decision.mjs';

const root = dirname(fileURLToPath(import.meta.url));
mkdirSync(join(root,'logs'), {recursive:true});
const socket = dgram.createSocket('udp4');
const seen = new Map();
let busy = false;
function record(event) {
  const row = {time:new Date().toISOString(), ...event};
  appendFileSync(join(root,'logs','decisions.jsonl'), JSON.stringify(row)+'\n');
  console.log(JSON.stringify(row));
}
socket.on('message', async (buffer, peer) => {
  if (peer.address !== '127.0.0.1' || buffer.length > 16384) return;
  let state;
  try { state = validateState(JSON.parse(buffer)); } catch { return; }
  if (state.event === 'telemetry') { record({event:'telemetry',state}); return; }
  const key = `${peer.port}:${state.request}:${state.nonce}`;
  if (seen.has(key)) {
    const reply=seen.get(key);
    if (reply) socket.send(reply, peer.port, peer.address);
    return;
  }
  if (busy) return;
  seen.set(key,null);
  if (seen.size > 128) seen.delete(seen.keys().next().value);
  busy=true;
  const start=performance.now();
  record({event:'request',state});
  try {
    const result = await decide(state, {apiKey:process.env.OPENROUTER_API_KEY});
    const reply=Buffer.from(wireReply(state,result.plan));
    seen.set(key,reply);
    socket.send(reply, peer.port, peer.address);
    record({event:'decision',request:state.request,nonce:state.nonce,latency_ms:Math.round(performance.now()-start),...result});
  } catch (error) {
    // Never log request headers, credentials, or raw provider errors.
    record({event:'fallback',request:state.request,error:error.message});
  } finally { busy=false; }
});
socket.on('error', error => { console.error(error.code); socket.close(); process.exitCode=1; });
socket.bind(28777,'127.0.0.1',()=>console.log('YaPB Jev bridge listening on 127.0.0.1:28777'));
