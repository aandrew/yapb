// Local HLDS RCON helper. Password comes from .env and is never logged.
import dgram from 'node:dgram';
export async function rcon(command, {port=27016, password=process.env.RCON_PASSWORD}={}) {
  if (!password) throw new Error('Missing RCON_PASSWORD');
  const socket=dgram.createSocket('udp4');
  const messages=[];
  let challenge;
  const result=await new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>{socket.close(); challenge ? resolve(messages.join('')) : reject(new Error('Local server did not respond'));},1800);
    socket.on('error',error=>{clearTimeout(timer);socket.close();reject(error);});
    socket.on('message',buffer=>{
      const text=buffer.subarray(4).toString();
      const match=text.match(/challenge rcon (\d+)/);
      if(match && !challenge){
        challenge=match[1];
        socket.send(Buffer.concat([Buffer.from([255,255,255,255]),Buffer.from(`rcon ${challenge} "${password}" ${command}\n`)]),port,'127.0.0.1');
      } else if(challenge) messages.push(text.replace(/^l/,'').replace(/\0/g,''));
    });
    socket.send(Buffer.concat([Buffer.from([255,255,255,255]),Buffer.from('challenge rcon\n')]),port,'127.0.0.1');
  });
  return result;
}
if(process.argv[1]?.endsWith('control.mjs')) {
  try {console.log(await rcon(process.argv.slice(2).join(' ')));}
  catch(error){console.error(error.message);process.exitCode=1;}
}
