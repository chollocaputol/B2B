// Cross-language fixture: requests emitted by the actual C++ journal, handled by
// the accepted TypeScript ingestion handler. Mock DB only, no external networking.
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {join} from 'node:path';
import {handleStudySf} from '../supabase/functions/_shared/study_sf_http.ts';
const root=process.env.STUDY_SF_FIXTURES;
if(!root)throw Error('STUDY_SF_FIXTURES required');
test('C++ retained journal batches: four types, 48 records, duplicate recovery, exact response',async()=>{
 const rows=new Map(),types=new Set();let accepted=0,duplicates=0;
 const deps={getRecorder:async token=>token==='test-device'?3:null,store:async(_,records)=>{const receipts=[];for(const r of records){const prev=rows.get(r.record_id);if(prev)assert.deepEqual(prev,r);rows.set(r.record_id,r);types.add(r.kind);receipts.push({record_id:r.record_id,status:prev?'duplicate':'accepted'});}return {data:receipts};}};
 for(let i=0;i<6;i++){
  const body=readFileSync(join(root,`request${i}.json`),'utf8');const parsed=JSON.parse(body);assert.equal(parsed.records.length,8);
  for(let retry=0;retry<2;retry++){
   const response=await handleStudySf(new Request('http://localhost/study',{method:'POST',headers:{authorization:'Bearer test-device','content-type':'application/json'},body}),deps);
   assert.equal(response.status,200);const raw=await response.text(),result=JSON.parse(raw);assert.equal(result.batch_id,parsed.batch_id);assert.deepEqual(result.records.map(r=>r.record_id),parsed.records.map(r=>r.record_id));assert.ok(result.records.every(r=>r.status===(retry?'duplicate':'accepted')));if(retry){duplicates+=8;writeFileSync(join(root,`response${i}.json`),raw);}else accepted+=8;
  }
 }
 assert.equal(rows.size,48);assert.equal(accepted,48);assert.equal(duplicates,48);assert.deepEqual([...types].sort(),['cancel','gps','sos','weather']);
});
