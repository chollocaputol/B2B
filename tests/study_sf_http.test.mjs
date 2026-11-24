import test from 'node:test';
import assert from 'node:assert/strict';
import {handleStudySf} from '../supabase/functions/_shared/study_sf_http.ts';
const session='aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa', batch='bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb';
const record=(i,kind,payload)=>({record_id:`${session}:${i}`,kind,recorder_node_id:3,captured_at:'2026-11-25T00:00:00Z',uptime_ms:i*100,payload});
const weather=record(1,'weather',{temperature_c:29,humidity_percent:78,pressure_hpa:1009,pressure_trend_hpa:-1,weather_state:1,micro:null});
const gps=record(2,'gps',{gps_fix:true,latitude:8.3,longitude:124.2});
function emergency(i,kind){const b=Buffer.alloc(62);b.writeUInt16LE(0x4d45);b[2]=1;b[3]=kind==='sos'?2:4;b.writeUInt32LE(2,4);b.writeUInt32LE(17,8);if(kind==='cancel'){b.writeUInt32LE(17,54);b.writeUInt32LE(2,58);}return record(i,kind,{origin_node_id:2,event_id:17,source:'received',packet_hex:b.toString('hex')});}
const sos=emergency(3,'sos'),cancel=emergency(4,'cancel');
const envelope=records=>({schema_version:1,batch_id:batch,records});
function fixture(){let rows=new Map();let calls=0;const deps={getRecorder:async t=>t==='v3'?3:null,store:async (v,records)=>{calls++;const next=new Map(rows),receipts=[];for(const r of records){const k=v+':'+r.record_id;const prev=next.get(k);if(prev&&JSON.stringify(prev)!==JSON.stringify(r))return {error:{code:'23505'}};next.set(k,r);receipts.push({record_id:r.record_id,status:prev?'duplicate':'accepted'});}rows=next;return {data:receipts};}};return {deps,get rows(){return rows;},get calls(){return calls;}};}
async function send(f,records=[weather,gps,sos,cancel],over={}){const request=new Request('http://localhost/study',{method:'POST',headers:{authorization:'Bearer v3','content-type':'application/json',...over.headers},body:over.raw??JSON.stringify(envelope(records))});const res=await handleStudySf(request,f.deps);return {status:res.status,body:await res.json()};}
test('four types, exact confirmations, duplicate replay after lost response',async()=>{const f=fixture();let r=await send(f);assert.equal(r.status,200);assert.equal(r.body.batch_id,batch);assert.deepEqual(r.body.records.map(x=>x.status),Array(4).fill('accepted'));assert.deepEqual(r.body.records.map(x=>x.record_id),[weather,gps,sos,cancel].map(x=>x.record_id));r=await send(f);assert.equal(r.status,200);assert.deepEqual(r.body.records.map(x=>x.status),Array(4).fill('duplicate'));assert.equal(f.rows.size,4);});
test('CANCEL first preserves both exact records for later reconciliation',async()=>{const f=fixture();assert.equal((await send(f,[cancel,sos])).status,200);assert.equal(f.rows.size,2);assert.equal([...f.rows.values()][0].payload.packet_hex,cancel.payload.packet_hex);});
test('conflicting identity rolls back mock transaction',async()=>{const f=fixture();await send(f,[weather]);const r=await send(f,[gps,{...weather,payload:{...weather.payload,temperature_c:31}}]);assert.equal(r.status,409);assert.equal(f.rows.size,1);});
for(const [name,records,over,status] of [
 ['missing token',[weather],{headers:{authorization:''}},401],['wrong credential',[weather],{headers:{authorization:'Bearer nope'}},403],
 ['wrong recorder',[{...weather,recorder_node_id:2}],{},422],['unknown type',[{...weather,kind:'ACK'}],{},422],
 ['no fix must have null coordinates',[{...gps,payload:{...gps.payload,gps_fix:false}}],{},422],
 ['invalid date',[{...weather,captured_at:'2026-02-30T00:00:00Z'}],{},422],['bad humidity',[{...weather,payload:{...weather.payload,humidity_percent:101}}],{},422],
 ['invalid cancel event',[{...cancel,payload:{...cancel.payload,event_id:18}}],{},422],['duplicate IDs inside batch',[weather,weather],{},422],
 ['oversize batch',Array.from({length:9},(_,i)=>({...weather,record_id:`${session}:${i+1}`})),{},422],['oversize body',[weather],{raw:' '.repeat(17000)},400],
 ['malformed body',[weather],{raw:'{'},400],['false source auth claim',[{...sos,source_auth:'VERIFIED'}],{},422],['wrong content type',[weather],{headers:{'content-type':'text/plain'}},415],
 ['missing exact raw bytes',[{...sos,payload:{...sos.payload,packet_hex:'abcd'}}],{},422],['wrong local source',[{...sos,payload:{...sos.payload,source:'local'}}],{},422],
 ['zero counter',[{...weather,record_id:session+':0'}],{},422],['counter overflow',[{...weather,record_id:session+':4294967296'}],{},422]
])test(name,async()=>{const f=fixture();assert.equal((await send(f,records,over)).status,status);assert.equal(f.calls,0);});
test('missing or reordered confirmations cannot be acknowledged',async()=>{const f=fixture();f.deps.store=async()=>({data:[{record_id:gps.record_id,status:'accepted'},{record_id:weather.record_id,status:'accepted'}]});assert.equal((await send(f,[weather,gps])).status,503);});
test('storage unavailable cannot be acknowledged',async()=>{const f=fixture();f.deps.store=async()=>{throw Error('offline');};assert.equal((await send(f,[weather])).status,503);});
test('missing GPS time and invalid measurements are explicitly null',async()=>{const f=fixture();const r=await send(f,[{...weather,captured_at:null,payload:{...weather.payload,temperature_c:null,humidity_percent:null,pressure_hpa:null,pressure_trend_hpa:null}},{...gps,captured_at:null,payload:{gps_fix:false,latitude:null,longitude:null}}]);assert.equal(r.status,200);});
