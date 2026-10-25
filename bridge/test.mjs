import {test} from 'node:test';
import assert from 'node:assert/strict';
import {plansFor, requestFor, decodeAnswer, wireReply, decide} from './decision.mjs';
const state={version:1,map:'de_dust2',request:7,nonce:123,bots:[1,3,5,7,9].map(id=>({id,origin:[id,0,0],health:100}))};
test('every offered plan has exactly three A and two B with no lost or duplicate players',()=>{
  const plans=Object.values(plansFor(state)); assert.equal(plans.length,10);
  for(const p of plans){assert.equal(p.a.length,3);assert.equal(p.b.length,2);assert.deepEqual([...p.a,...p.b].sort((a,b)=>a-b),[1,3,5,7,9]);}
});
test('reject malformed rosters and fabricated model choices',()=>{
  assert.throws(()=>plansFor({...state,bots:[...state.bots.slice(0,4),state.bots[0]]}));
  assert.throws(()=>decodeAnswer(state,{answers:{deployment:{type:'choice',choice:'invented'}}}));
  assert.throws(()=>wireReply(state,{a:[1,3],b:[5,7,9]}));
});
test('provider adapter sends state and maps a typed choice to the game protocol',async()=>{
  const result=await decide(state,{apiKey:'test-key',fetchFn:async(url,options)=>{
    assert.equal(url,'https://openrouter.ai/api/alpha/decisions');
    assert.deepEqual(JSON.parse(options.body),requestFor(state));
    return {ok:true,json:async()=>({model:'typesafe/jev-1.14',answers:{deployment:{type:'choice',choice:'a_1_5_9'}}})};
  }});
  assert.equal(wireReply(state,result.plan),'YJ1 7 123 1 5 9');
});
test('provider failures propagate to fallback',async()=>{
  await assert.rejects(decide(state,{apiKey:'test-key',fetchFn:async()=>({ok:false,status:429})}),/HTTP 429/);
});
