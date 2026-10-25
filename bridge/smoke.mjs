import {decide} from './decision.mjs';
const state = {version:1,map:'de_dust2',request:1,nonce:1,
  sites:{A:[1152,2464,144],B:[-1536,2688,48]},
  bots:[1,2,3,4,5].map(id=>({id,health:100,origin:[id*60,2200,0]}))};
const start=performance.now();
try {
  const result=await decide(state,{apiKey:process.env.OPENROUTER_API_KEY,timeout:10000});
  console.log(JSON.stringify({event:'live-smoke',latency_ms:Math.round(performance.now()-start),...result}));
} catch(error) { console.error(error.message); process.exitCode=1; }
