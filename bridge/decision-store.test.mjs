import {test} from 'node:test';
import assert from 'node:assert/strict';
import {mkdtempSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {DecisionStore,validateAppliedOrder} from './decision-store.mjs';
const context={server_session:7,map:'de_dust2',team:'T',round:2,request:3,nonce:99,roster_revision:1};
const assignments=Object.fromEntries([1,2,3,4,5].map(i=>[`t_0${i}`,`t_a_2_${i}`]));
const order=(over={})=>({version:1,event:'order_applied',...context,source:'jev',phase:7,target_site:'A',game_time:20,assignments,...over});
const row=(event,extra={})=>({time:'2026-10-25T10:00:00Z',event,...context,...extra});
test('SQLite retains exact input, offered choices, reply and independent native acceptance across reopen',()=>{
 const dir=mkdtempSync(join(tmpdir(),'jev-db-')),file=join(dir,'test.sqlite');
 try{
  let db=new DecisionStore(file);
  db.record(row('request',{state:{...context,round_state:{phase_code:7}}}));
  db.record(row('model_request',{payload:{state:{briefing:'Four versus four.'},questions:{deployment:{criteria:{hold:'Keep positions.'}}}}}));
  db.record(row('decision',{choice:'hold',strategy:null,latency_ms:500,plan:{assignments}}));
  assert.equal(db.list()[0].status,'reply_sent');
  const ack={time:'2026-10-25T10:00:01Z',event:'order_applied',order:validateAppliedOrder(order())};db.record(ack);db.record(ack);
  assert.equal(db.stats().applied_orders,1);assert.equal(db.list()[0].status,'accepted');
  db.close();db=new DecisionStore(file);
  const detail=db.detail(db.list()[0].id);assert.equal(detail.model_request.state.briefing,'Four versus four.');assert.equal(detail.applied_orders[0].source,'jev');assert.deepEqual(detail.result.plan.assignments,assignments);
  db.record(row('fallback',{server_session:8,error:'timeout'}));assert.equal(db.list({team:'T',limit:1})[0].status,'fallback');assert.equal(db.stats().decisions,2);
  db.close();
 }finally{rmSync(dir,{recursive:true,force:true});}
});
test('automatic orders remain distinct from Jev selections and malformed orders are rejected',()=>{
 const db=new DecisionStore(':memory:');db.record(row('request',{state:context}));
 db.record({time:new Date().toISOString(),event:'order_applied',order:validateAppliedOrder(order({source:'recovery_choice_fallback'}))});
 assert.equal(db.list()[0].status,'requested');assert.equal(db.stats().applied_orders,1);
 assert.throws(()=>validateAppliedOrder(order({assignments:{bad:'bad'}})));
 db.record(row('telemetry',{state:{...context,marker:'later state'}}));assert.equal(db.stats().events,3);db.close();
});
