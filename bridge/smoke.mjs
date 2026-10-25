import {decide} from './decision.mjs';
import {fixture} from './fixture.mjs';
const state=fixture();
const start=performance.now();
try {
  const result=await decide(state,{apiKey:process.env.OPENROUTER_API_KEY,timeout:10000});
  console.log(JSON.stringify({event:'live-smoke',latency_ms:Math.round(performance.now()-start),...result}));
} catch(error) { console.error(error.message); process.exitCode=1; }
