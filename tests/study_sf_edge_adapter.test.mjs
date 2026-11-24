// Exact entry point + accepted handler, mocked local HTTP boundary. No live Auth or database claim.
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {pathToFileURL} from 'node:url';
const entry=process.env.SF_EDGE_ENTRY;
assert.ok(entry);
const fixture=JSON.parse(readFileSync(new URL('./fixtures/study_sf/request0.json',import.meta.url),'utf8'));
const defaults={THESIS_SF_LOCAL_LAB:'1',SUPABASE_URL:'http://kong:8000',SUPABASE_ANON_KEY:'test-public',SUPABASE_SERVICE_ROLE_KEY:'test-server'};
let env={...defaults},handler,calls=[],auth,rpc;
globalThis.Deno={env:{get:k=>env[k]},serve:fn=>{handler=fn}};
const originalFetch=globalThis.fetch;
globalThis.fetch=async (url,opts)=>{
 calls.push({url,opts});
 assert.equal(opts.redirect,'error');assert.ok(opts.signal instanceof AbortSignal);
 if(url==='http://kong:8000/auth/v1/user')return auth(opts);
 if(url==='http://kong:8000/rest/v1/rpc/study_sf_ingest')return rpc(opts);
 assert.fail('Unexpected endpoint');
};
await import(pathToFileURL(entry).href);assert.equal(typeof handler,'function');
const reply=(body,status=200)=>Response.json(body,{status});
const validUser={id:'synthetic-user',app_metadata:{thesis_study_device:true,thesis_node_id:3}};
const receipts=fixture.records.map(r=>({record_id:r.record_id,status:'accepted'}));
function reset(){env={...defaults};calls=[];auth=()=>reply(validUser);rpc=()=>reply(receipts);}
async function send(method='POST',token='vessel-token',batch=fixture){const r=await handler(new Request('http://localhost/functions/v1/ingest-study-sf',{method,headers:{authorization:`Bearer ${token}`,'content-type':'application/json'},...(method==='POST'?{body:JSON.stringify(batch)}:{})}));return {status:r.status,data:await r.json()};}
async function expectStatus(status,error){const r=await send();assert.equal(r.status,status);assert.equal(r.data.error,error);}
test('disabled lab, hosted URL, absent or identical keys never make HTTP calls',async()=>{for(const changes of [{THESIS_SF_LOCAL_LAB:'0'},{SUPABASE_URL:'https://example.invalid'},{SUPABASE_ANON_KEY:''},{SUPABASE_SERVICE_ROLE_KEY:''},{SUPABASE_SERVICE_ROLE_KEY:'test-public'}]){reset();Object.assign(env,changes);await expectStatus(503,'study_lab_disabled');assert.equal(calls.length,0);}});
test('GET readiness is 405, without outbound requests',async()=>{reset();assert.deepEqual(await send('GET'),{status:405,data:{error:'method_not_allowed'}});assert.equal(calls.length,0);});
test('server user lookup and exact RPC payload, correct separation of credentials',async()=>{reset();const r=await send();assert.equal(r.status,200);assert.deepEqual(r.data,{schema_version:1,batch_id:fixture.batch_id,recorder_node_id:3,records:receipts});assert.equal(calls.length,2);assert.deepEqual(calls[0].opts.headers,{apikey:'test-public',authorization:'Bearer vessel-token'});assert.equal(calls[0].opts.method,'GET');assert.equal(calls[1].opts.method,'POST');assert.equal(calls[1].opts.headers.apikey,'test-server');assert.equal(calls[1].opts.headers.authorization,'Bearer test-server');assert.deepEqual(JSON.parse(calls[1].opts.body),{p_recorder:3,p_records:fixture.records});});
test('duplicate receipts passed unchanged',async()=>{reset();const duplicates=receipts.map(r=>({...r,status:'duplicate'}));rpc=()=>reply(duplicates);const r=await send();assert.equal(r.status,200);assert.deepEqual(r.data.records,duplicates);});
test('user metadata spoof, reviewer and malformed node are forbidden before RPC',async()=>{for(const user of [{id:'u',app_metadata:{},user_metadata:validUser.app_metadata},{id:'r',app_metadata:{thesis_study_reviewer:true}},{id:'u',app_metadata:{thesis_study_device:true,thesis_node_id:'3'}},{id:'u',app_metadata:{thesis_study_device:true,thesis_node_id:10000}}]){reset();auth=()=>reply(user);await expectStatus(403,'device_not_authorized');assert.equal(calls.length,1);}});
test('Auth 401 or 403 denies without RPC',async()=>{for(const s of [401,403]){reset();auth=()=>reply({},s);await expectStatus(403,'device_not_authorized');assert.equal(calls.length,1);}});
test('Auth network, malformed response and upstream error are unavailable, not authorization passes',async()=>{for(const f of [()=>{throw Error('offline')},()=>reply({},500),()=>reply({}),()=>new Response('not JSON')]){reset();auth=f;await expectStatus(503,'auth_unavailable');assert.equal(calls.length,1);}});
test('wrong recorder rejected before RPC',async()=>{reset();auth=()=>reply({id:'u',app_metadata:{thesis_study_device:true,thesis_node_id:2}});await expectStatus(422,'invalid_batch');assert.equal(calls.length,1);});
test('database identity conflict maps to 409 only for actual SQLSTATE 23505',async()=>{reset();rpc=()=>reply({code:'23505'},409);await expectStatus(409,'record_identity_conflict');reset();rpc=()=>reply({code:'23505'},500);await expectStatus(503,'store_unavailable');});
test('RPC failure never confirms a record',async()=>{for(const f of [()=>reply({},500),()=>reply({code:'42501'},403),()=>{throw Error('offline')},()=>new Response('bad',{status:502})]){reset();rpc=f;await expectStatus(503,'store_unavailable');}});
test('malformed or out-of-order success confirmation fails closed',async()=>{for(const body of [null,[],[...receipts].reverse(),{records:receipts}]){reset();rpc=()=>reply(body);await expectStatus(503,'invalid_storage_confirmation');}});
test.after(()=>{globalThis.fetch=originalFetch;delete globalThis.Deno;});
