// Hosted and local boundary regression, using the accepted C++ request fixture.
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const hosted='https://twqumqiplqqfhixjvxeo.supabase.co';
const local='http://kong:8000';
const fixture=JSON.parse(readFileSync(new URL('./fixtures/study_sf/request0.json',import.meta.url),'utf8'));
const defaults={SUPABASE_URL:hosted,SUPABASE_ANON_KEY:'test-public',SUPABASE_SERVICE_ROLE_KEY:'test-server'};
let env, handler, calls, user, authStatus, rpcStatus, receiptStatus;
globalThis.Deno={env:{get:k=>env[k]},serve:fn=>{handler=fn}};
const originalFetch=globalThis.fetch;
globalThis.fetch=async (url,opts)=>{
 calls.push({url,opts});assert.equal(opts.redirect,'error');assert.ok(opts.signal instanceof AbortSignal);
 if(url===env.SUPABASE_URL+'/auth/v1/user'){
  assert.equal(opts.headers.apikey,'test-public');assert.equal(opts.headers.authorization,'Bearer device-token');
  return Response.json(user,{status:authStatus});
 }
 if(url===env.SUPABASE_URL+'/rest/v1/rpc/study_sf_ingest'){
  assert.equal(opts.headers.apikey,'test-server');assert.equal(opts.headers.authorization,'Bearer test-server');
  assert.deepEqual(JSON.parse(opts.body),{p_recorder:3,p_records:fixture.records});
  return Response.json(rpcStatus===409?{code:'23505'}:fixture.records.map(r=>({record_id:r.record_id,status:receiptStatus})),{status:rpcStatus});
 }
 assert.fail('Unexpected upstream URL');
};
await import('../supabase/functions/ingest-study-sf/index.ts');
function reset(){env={...defaults};calls=[];user={id:'device',app_metadata:{thesis_study_device:true,thesis_node_id:3}};authStatus=200;rpcStatus=200;receiptStatus='accepted';}
async function send(token='device-token'){
 const r=await handler(new Request(hosted+'/functions/v1/ingest-study-sf',{method:'POST',headers:{'content-type':'application/json',...(token?{authorization:'Bearer '+token}:{})},body:JSON.stringify(fixture)}));
 return {status:r.status,data:await r.json()};
}
test('approved hosted URL: exact accepted and duplicate receipts',async()=>{reset();for(const status of ['accepted','duplicate']){receiptStatus=status;const r=await send();assert.equal(r.status,200);assert.deepEqual(r.data,{schema_version:1,batch_id:fixture.batch_id,recorder_node_id:3,records:fixture.records.map(x=>({record_id:x.record_id,status}))});}assert.equal(calls.length,4);});
test('local lab still requires explicit enablement',async()=>{reset();env.SUPABASE_URL=local;assert.equal((await send()).status,503);assert.equal(calls.length,0);env.THESIS_SF_LOCAL_LAB='1';assert.equal((await send()).status,200);});
test('unapproved destination or absent/equal credentials fail before fetch',async()=>{for(const change of [{SUPABASE_URL:'https://example.invalid'},{SUPABASE_URL:hosted+'.example.invalid'},{SUPABASE_URL:''},{SUPABASE_ANON_KEY:''},{SUPABASE_SERVICE_ROLE_KEY:''},{SUPABASE_SERVICE_ROLE_KEY:'test-public'}]){reset();Object.assign(env,change);assert.equal((await send()).status,503);assert.equal(calls.length,0);}});
test('no bearer, denied Auth, reviewer, mixed roles, spoofed metadata and wrong node never ingest',async()=>{reset();assert.equal((await send('')).status,401);assert.equal(calls.length,0);reset();authStatus=401;assert.equal((await send()).status,403);assert.equal(calls.length,1);for(const metadata of [{thesis_study_reviewer:true},{thesis_study_device:true,thesis_study_reviewer:true,thesis_node_id:3},{},{thesis_study_device:true,thesis_node_id:2}]){reset();user.app_metadata=metadata;user.user_metadata={thesis_study_device:true,thesis_node_id:3};assert.ok([403,422].includes((await send()).status));assert.equal(calls.length,1);}});
test('actual SQL conflict remains a rejection',async()=>{reset();rpcStatus=409;assert.deepEqual(await send(),{status:409,data:{error:'record_identity_conflict'}});});
test.after(()=>{globalThis.fetch=originalFetch;delete globalThis.Deno;});
