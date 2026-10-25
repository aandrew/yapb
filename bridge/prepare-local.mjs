import {readFileSync,writeFileSync,mkdirSync,existsSync,cpSync,copyFileSync} from 'node:fs';
import {randomBytes,createHash} from 'node:crypto';
import {dirname,resolve,join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {loadConfig,engineConfig} from './config.mjs';
const bridge=dirname(fileURLToPath(import.meta.url));
const repo=resolve(bridge,'..');
const workspace=resolve(repo,'..');
const runtime=join(workspace,'work','runtime','Half-Life');
const mod=join(runtime,'cstrike');
if(!existsSync(join(runtime,'hlds.exe'))) throw new Error('Copy the local Steam Half-Life installation into work/runtime/Half-Life first');
const envPath=join(bridge,'.env');
let env=existsSync(envPath)?readFileSync(envPath,'utf8').replace(/^\uFEFF/,''):'OPENROUTER_API_KEY=\n';
let password=env.match(/^RCON_PASSWORD=(\S+)/m)?.[1];
if(!password){password=randomBytes(24).toString('hex');env+=`\nRCON_PASSWORD=${password}\n`;writeFileSync(envPath,env);}
const config=join(mod,'liblist.gam');
if(!existsSync(config+'.pre-jev'))copyFileSync(config,config+'.pre-jev');
writeFileSync(config,readFileSync(config,'utf8').replace(/^gamedll\s+"[^"]+"/m,'gamedll "addons\\yapb\\bin\\yapb.dll"'));
cpSync(join(repo,'cfg','addons','yapb'),join(mod,'addons','yapb'),{recursive:true});
for(const dir of ['bin','data/graph','data/train','data/pwf','logs'])mkdirSync(join(mod,'addons','yapb',dir),{recursive:true});
copyFileSync(join(repo,'build','local','yapb.dll'),join(mod,'addons','yapb','bin','yapb.dll'));
copyFileSync(join(workspace,'work','assets','de_dust2.graph'),join(mod,'addons','yapb','data','graph','de_dust2.graph'));
const catalogue=JSON.parse(readFileSync(join(bridge,'config','de_dust2.areas.json'),'utf8'));
const graphHash=createHash('sha256').update(readFileSync(join(workspace,'work','assets','de_dust2.graph'))).digest('hex');
if(graphHash!==catalogue.graph_sha256)console.warn('Callout waypoint IDs need regeneration for this graph. Spatial callout boundaries remain drafts.');
const {roster,map}=loadConfig();
writeFileSync(join(mod,'addons','yapb','conf','systemone.cfg'),engineConfig(roster,map));
// Dedicated development configuration only; the normal Steam installation is untouched.
const server=`hostname "YaPB Jev Local MVP"\nsv_lan 1\nrcon_password "${password}"\nmp_autoteambalance 0\nmp_limitteams 0\nmp_freezetime 8\nmp_roundtime 3\nmp_timelimit 0\nmp_startmoney 16000\nmp_logmessages 1\nlog on\nyb_systemone 1\nyb_quota 0\nyb_autovacate 0\nyb_kick_after_player_connect 0\nyb_save_bots 0\nyb_threadpool_workers 0\nyb_chat 1\nyb_radio_mode 0\nyb_user_follow_percent 0\nyb_display_welcome_text 0\nyb_graph_url ""\nyb_graph_url_upload ""\n`;
writeFileSync(join(mod,'jev_local.cfg'),server);
writeFileSync(join(mod,'server.cfg'),server);
// Replace existing entries instead of duplicating them: YaPB preserves CVARs
// that already differ from a config's earlier value.
const overrides=server.split('\n').filter(line=>line.startsWith('yb_'));
let botConfig=readFileSync(join(mod,'addons','yapb','conf','yapb.cfg'),'utf8');
for(const line of overrides){
  const key=line.split(' ')[0];
  const pattern=new RegExp(`^${key}\\s+.*$`,'gm');
  botConfig=pattern.test(botConfig)?botConfig.replace(pattern,line):botConfig+'\n'+line;
}
writeFileSync(join(mod,'addons','yapb','conf','yapb.cfg'),botConfig+'\n');
console.log('Prepared isolated local runtime and backed up liblist.gam. Credentials are stored only in ignored local files.');
