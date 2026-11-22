// Local loopback HTTP tests; real handler, MOCK auth and archive RPC.
import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {Readable} from 'node:stream';
import {once} from 'node:events';
import {createHash} from 'node:crypto';
import {handleF06HistoryRequest} from '../supabase/functions/_shared/f06_history_http.ts';
const actor='00000000-0000-4000-8000-000000000001';
const line=(i,k='sos_rx',v=2,x='')=>'{'+'"kind":"'+k+'","vessel_id":'+v+',"boot_token":12,"event_index":'+i+',"uptime_ms":123'+x+'}\n';
const sos=[line(1),line(2,'sos_ack_tx'),line(3,'cancel_rx'),line(4,'cancel_ack_tx')].join('');
const payload=(text=sos,patch={})=>({schema_version:1,vessel_id:2,session:'S58BAFE2',journal:'sos_events.jsonl',file_sha256:createHash('sha256').update(text).digest('hex').toUpperCase(),content_base64:Buffer.from(text).toString('base64'),...patch});
async function harness(fn,override={}){
 const approvals=new Set(), files=new Map(), events=new Set();let calls=0;
 const approve=p=>approvals.add([p.vessel_id,p.session,p.file_sha256,Buffer.from(p.content_base64,'base64').length,Buffer.from(p.content_base64,'base64').toString().split('\n').filter(Boolean).length].join(':'));
 const db=async a=>{
  calls++;
  const key=[a.p_vessel,a.p_session,a.p_sha256,a.p_size,a.p_records.length].join(':');
  if(!approvals.has(key))return {error:{code:'P0001'}};
  const fileKey=a.p_vessel+':'+a.p_session;
  if(files.has(fileKey))return files.get(fileKey)===key?{data:{status:'duplicate',record_count:a.p_records.length,scope:'historical_sos_archive_only',emergency_alerts_changed:false}}:{error:{code:'P0001'}};
  const ids=a.p_records.map(r=>a.p_vessel+':'+r.boot_token+':'+r.event_index);
  if(ids.some(id=>events.has(id)))return {error:{code:'23505'}};
  files.set(fileKey,key);ids.forEach(id=>events.add(id));
  return {data:{status:'accepted',record_count:a.p_records.length,scope:'historical_sos_archive_only',emergency_alerts_changed:false}};
 };
 const deps={getOperator:async t=>t==='operator'?{id:actor,archiveImporter:true}:t==='viewer'?{id:actor,archiveImporter:false}:null,archiveFile:db,...override};
 const server=createServer(async(req,res)=>{
  try{
   const request=new Request('http://127.0.0.1:'+server.address().port+req.url,{method:req.method,headers:req.headers,...(req.method==='GET'?{}:{body:Readable.toWeb(req),duplex:'half'})});
   const out=await handleF06HistoryRequest(request,deps);
   res.writeHead(out.status,Object.fromEntries(out.headers));res.end(Buffer.from(await out.arrayBuffer()));
  }catch(e){res.writeHead(500);res.end(String(e));}
 });
 server.listen(0,'127.0.0.1');await once(server,'listening');
 const send=async (p=payload(),o={})=>{
  const resp=await fetch('http://127.0.0.1:'+server.address().port+'/functions/v1/ingest-f06-history',{method:o.method??'POST',headers:{authorization:'Bearer '+(o.token??'operator'),'content-type':o.type??'application/json',...o.headers},...(o.method==='GET'?{}:{body:o.raw??JSON.stringify(p)})});
  return {status:resp.status,data:await resp.json(),headers:resp.headers};
 };
 try{await fn({send,approve,files,events,get calls(){return calls;}});}finally{await new Promise((ok,bad)=>server.close(e=>e?bad(e):ok()));}
}
const cases=[
 ['1 non-POST',405,{}, {method:'GET'}],
 ['2 missing authorization',401,{}, {headers:{authorization:''}}],
 ['3 invalid token',401,{}, {token:'invalid'}],
 ['4 unauthorized user',403,{}, {token:'viewer'}],
 ['5 wrong content type',415,{}, {type:'text/plain'}],
 ['6 invalid JSON',400,{}, {raw:'{"a":bad'}],
 ['7 bad schema version',400,{schema_version:9}],
 ['8 bad journal name',400,{journal:'telemetry.jsonl'}],
 ['9 file checksum mismatch',422,{file_sha256:'F'.repeat(64)}],
 ['10 invalid Base64',400,{content_base64:'%%%'}],
 ['11 wrong vessel identity',422,{}, {},line(1,'sos_rx',3)],
 ['12 non-SOS radio event',422,{}, {},line(1,'telemetry_tx')],
 ['13 incomplete JSONL',422,{}, {},line(1).slice(0,-1)],
 ['14 mixed boot LEGACY',422,{session:'LEGACY'}],
 ['15 nested JSON',422,{}, {},line(1,'sos_rx',2,',"x":{"a":1}')],
 ['16 duplicate JSON property',422,{}, {},line(1,'sos_rx',2,',"kind":"sos_rx"')],
 ['17 unapproved file',409],
 ['18 malformed operator id',503,{}, {},undefined,{getOperator:async()=>({id:'bad',archiveImporter:true})}],
 ['19 unavailable auth',503,{}, {},undefined,{getOperator:async()=>{throw Error('auth down')}}],
 ['20 unavailable database',503,{}, {},undefined,{archiveFile:async()=>{throw Error('secret')}}],
 ['21 false successful RPC response',503,{}, {},undefined,{archiveFile:async()=>({data:{status:'accepted',record_count:4,scope:'historical_sos_archive_only',emergency_alerts_changed:true}})}],
 ['22 oversized HTTP body',413,{padding:'X'.repeat(110000)}],
];
for(const [name,status,patch={},opts={},contents=sos,overrides={}] of cases){
 test(name,async()=>harness(async({send})=>{
   const response=await send(payload(contents,patch),opts);assert.equal(response.status,status,name);
 },overrides));
}
test('23 approved 4-event archive',async()=>harness(async({send,approve,files,events})=>{
 const p=payload();approve(p);const r=await send(p);assert.equal(r.status,200);assert.equal(r.data.result.status,'accepted');assert.equal(r.data.result.record_count,4);assert.equal(r.data.upload_authorization,'history_only');assert.equal(files.size,1);assert.equal(events.size,4);
}));
test('24 identical retry no duplicates',async()=>harness(async({send,approve,files,events})=>{
 const p=payload();approve(p);assert.equal((await send(p)).data.result.status,'accepted');assert.equal((await send(p)).data.result.status,'duplicate');assert.equal(files.size,1);assert.equal(events.size,4);
}));
test('25 altered and separately approved file cannot replace session',async()=>harness(async({send,approve,events})=>{
 const p=payload(),q=payload(sos.replace('123','124'));approve(p);approve(q);assert.equal((await send(p)).status,200);assert.equal((await send(q)).status,409);assert.equal(events.size,4);
}));
test('26 cross-session collision denied and no live SOS created',async()=>harness(async({send,approve,files,events})=>{
 const p=payload(line(1)),q=payload(line(1),{session:'SB1F7223'});approve(p);approve(q);assert.equal((await send(p)).status,200);assert.equal((await send(q)).status,409);assert.equal(files.size,1);assert.equal(events.size,1);
}));
