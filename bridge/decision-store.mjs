import {DatabaseSync} from 'node:sqlite';
import {createHash} from 'node:crypto';
const key=r=>[r.server_session,r.map,r.team,r.round,r.request,r.nonce,r.roster_revision];
export class DecisionStore {
 constructor(path){
  this.db=new DatabaseSync(path);
  this.db.exec(`PRAGMA journal_mode=WAL; PRAGMA busy_timeout=2000;
   CREATE TABLE IF NOT EXISTS events(id INTEGER PRIMARY KEY, fingerprint TEXT UNIQUE NOT NULL, time TEXT NOT NULL, event TEXT NOT NULL, server_session INTEGER, map TEXT, team TEXT, round INTEGER, request INTEGER, payload TEXT NOT NULL);
   CREATE INDEX IF NOT EXISTS events_round ON events(server_session,map,round,team,id);
   CREATE TABLE IF NOT EXISTS decisions(id INTEGER PRIMARY KEY, server_session INTEGER NOT NULL,map TEXT NOT NULL,team TEXT NOT NULL,round INTEGER NOT NULL,request INTEGER NOT NULL,nonce INTEGER NOT NULL,roster_revision INTEGER NOT NULL,created_at TEXT NOT NULL,status TEXT NOT NULL DEFAULT 'requested',choice TEXT,strategy TEXT,latency_ms INTEGER,error TEXT,state_json TEXT,model_request_json TEXT,result_json TEXT, UNIQUE(server_session,map,team,round,request,nonce,roster_revision));
   CREATE TABLE IF NOT EXISTS applied_orders(id INTEGER PRIMARY KEY,event_id INTEGER UNIQUE NOT NULL,server_session INTEGER,map TEXT,team TEXT,round INTEGER,request INTEGER,nonce INTEGER,roster_revision INTEGER,source TEXT,phase INTEGER,target_site TEXT,game_time REAL,assignments_json TEXT, FOREIGN KEY(event_id) REFERENCES events(id));
   PRAGMA user_version=1;`);
  this.insert=this.db.prepare('INSERT OR IGNORE INTO events(fingerprint,time,event,server_session,map,team,round,request,payload) VALUES(?,?,?,?,?,?,?,?,?)');
 }
 record(row){
  const state=row.state,packet=row.order??row.report,context=state??packet??row;
  const r={...row,server_session:context.server_session,map:context.map,team:context.team,round:context.round,request:context.request??row.request_id??row.request,nonce:context.nonce,roster_revision:context.roster_revision};
  const raw=JSON.stringify(row),hash=createHash('sha256').update(raw).digest('hex');
  this.db.exec('BEGIN');
  try{
   const result=this.insert.run(hash,row.time??new Date().toISOString(),row.event,r.server_session??null,r.map??null,r.team??null,r.round??null,r.request??null,raw);
   if(!result.changes){this.db.exec('COMMIT');return;}
   if(['request','model_request','decision','fallback'].includes(row.event)&&key(r).every(x=>x!==undefined&&x!==null)){
    this.db.prepare('INSERT OR IGNORE INTO decisions(server_session,map,team,round,request,nonce,roster_revision,created_at) VALUES(?,?,?,?,?,?,?,?)').run(...key(r),row.time);
    const where=' WHERE server_session=? AND map=? AND team=? AND round=? AND request=? AND nonce=? AND roster_revision=?';
    if(row.event==='request')this.db.prepare('UPDATE decisions SET state_json=?'+where).run(JSON.stringify(state),...key(r));
    if(row.event==='model_request')this.db.prepare('UPDATE decisions SET model_request_json=?'+where).run(JSON.stringify(row.payload),...key(r));
    if(row.event==='decision')this.db.prepare("UPDATE decisions SET status=CASE WHEN status='accepted' THEN status ELSE 'reply_sent' END,choice=?,strategy=?,latency_ms=?,result_json=?"+where).run(row.choice,JSON.stringify(row.strategy??null),row.latency_ms,JSON.stringify(row),...key(r));
    if(row.event==='fallback')this.db.prepare("UPDATE decisions SET status='fallback',error=?"+where).run(row.error,...key(r));
   }
   if(row.event==='order_applied'){
    const o=row.order;
    this.db.prepare('INSERT INTO applied_orders(event_id,server_session,map,team,round,request,nonce,roster_revision,source,phase,target_site,game_time,assignments_json) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)').run(Number(result.lastInsertRowid),o.server_session,o.map,o.team,o.round,o.request,o.nonce,o.roster_revision,o.source,o.phase,o.target_site,o.game_time,JSON.stringify(o.assignments));
    if(o.source==='jev')this.db.prepare("UPDATE decisions SET status='accepted' WHERE server_session=? AND map=? AND team=? AND round=? AND request=? AND nonce=? AND roster_revision=?").run(...key(o));
   }
   this.db.exec('COMMIT');
  }catch(error){this.db.exec('ROLLBACK');throw error;}
 }
 list({team,round,limit=50}={}){
  const where=[],args=[];if(['CT','T'].includes(team)){where.push('team=?');args.push(team);}if(Number.isSafeInteger(round)&&round>=0){where.push('round=?');args.push(round);}
  return this.db.prepare('SELECT id,created_at,server_session,map,team,round,request,status,choice,strategy,latency_ms,error FROM decisions'+(where.length?' WHERE '+where.join(' AND '):'')+' ORDER BY id DESC LIMIT ?').all(...args,Math.floor(Math.min(200,Math.max(1,Number(limit)||50))));
 }
 detail(id){
  const row=this.db.prepare('SELECT * FROM decisions WHERE id=?').get(id);if(!row)return null;
  for(const k of ['state_json','model_request_json','result_json']){row[k.replace('_json','')]=row[k]?JSON.parse(row[k]):null;delete row[k];}
  row.applied_orders=this.db.prepare('SELECT * FROM applied_orders WHERE server_session=? AND map=? AND team=? AND round=? AND request=? AND nonce=? AND roster_revision=? ORDER BY id').all(...key(row)).map(o=>({...o,assignments:JSON.parse(o.assignments_json),assignments_json:undefined}));return row;
 }
 stats(){return {decisions:this.db.prepare('SELECT count(*) n FROM decisions').get().n,events:this.db.prepare('SELECT count(*) n FROM events').get().n,applied_orders:this.db.prepare('SELECT count(*) n FROM applied_orders').get().n};}
 close(){this.db.close();}
}
export function validateAppliedOrder(o){
 if(o?.version!==1||o.event!=='order_applied'||!['CT','T'].includes(o.team)||!/^[a-z][a-z0-9_]{0,30}$/.test(o.map)||!['server_session','round','request','nonce','roster_revision','phase'].every(k=>Number.isSafeInteger(o[k])&&o[k]>=0)||!Number.isFinite(o.game_time)||typeof o.source!=='string'||o.source.length>48||!['A','B','M','S','L','-'].includes(o.target_site)||!o.assignments||Object.keys(o.assignments).length!==5||Object.entries(o.assignments).some(([id,p])=>!/^[a-z][a-z0-9_]{0,30}$/.test(id)||!/^[a-z][a-z0-9_]{0,30}$/.test(p)))throw Error('invalid applied order');
 return o;
}
