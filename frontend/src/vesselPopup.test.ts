import { test } from 'node:test';
import assert from 'node:assert/strict';
import { popupEmergency, popupBattery, emergencyReports, popupEmergencyFromReports, summarizeEmergencyReports, reportTime } from './vesselPopup.ts';
import type { Row } from './studySf.ts';
import type { Reading } from './data.ts';
const now=Date.parse('2026-11-27T00:00:00Z'), earlier=now-1000;
function row(kind:'sos'|'cancel',origin=2,event=9,time:number|null=earlier,recorder=3):Row {
 return {recorder_node_id:recorder,record_id:'test:1',synced_at:new Date(now).toISOString(),record_json:{recorder_node_id:recorder,record_id:'test:1',kind,captured_at:time===null?null:new Date(time).toISOString(),uptime_ms:0,payload:{origin_node_id:origin,event_id:event}}};
}
test('popup associates received SOS with origin rather than recorder',()=>{
 assert.deepEqual(popupEmergency('2',now,[row('sos')],[]),{status:'Needs attention',observedAt:earlier});
 assert.equal(popupEmergency('3',now,[row('sos')],[]).status,'No SOS recorded');
});
test('popup cancellation dominates corresponding event regardless of order or recorder',()=>{
 const records=[row('cancel',2,9,null,2),row('sos')];
 assert.equal(popupEmergency('2',now,records,[]).status,'Cancellation observed');
 assert.equal(popupEmergency('2',now,records.reverse(),[]).status,'Cancellation observed');
});
test('popup other event cancellation does not clear attention',()=>{
 assert.equal(popupEmergency('2',now,[row('sos'),row('cancel',2,10)],[]).status,'Needs attention');
});
test('popup unknown or future capture never substitutes receipt time',()=>{
 for(const time of [null,now+1000])assert.deepEqual(popupEmergency('2',now,[row('sos',2,9,time)],[]),{status:'Needs attention',observedAt:null});
});
test('popup repeated observation does not replace earliest known event observation',()=>{
 assert.equal(popupEmergency('2',now,[row('sos'),row('sos',2,9,now)],[]).observedAt,earlier);
});
test('popup latest known open event is selected without time-based expiry',()=>{
 assert.equal(popupEmergency('2',now,[row('sos',2,9,now-99999999),row('sos',2,10,earlier)],[]).observedAt,earlier);
 assert.equal(popupEmergency('2',now,[row('sos',2,9,now-99999999)],[]).status,'Needs attention');
});
test('popup sample SOS is separate from hosted records',()=>{
 const sample=[{deviceId:'2',timestamp:earlier,latitude:8,longitude:124}];
 assert.equal(popupEmergency('2',now,undefined,sample).status,'Needs attention');
 assert.equal(popupEmergency('2',now,[],sample).status,'No SOS recorded');
});
test('popup battery preserves zero and rejects unavailable or invalid values',()=>{
 for(const v of [null,undefined,NaN,-1,101])assert.equal(popupBattery({battery:v} as Reading),'Unavailable');
 assert.equal(popupBattery({battery:0} as Reading),'0 %');
 assert.equal(popupBattery({battery:86} as Reading),'86 %');
 assert.equal(popupBattery(null),'Unavailable');
});

// FE2D: one classification for marker, popup and Overview summary.
const summary=(records:Row[])=>summarizeEmergencyReports(emergencyReports(now,records,[]));
test('summary prioritizes old outstanding SOS over newer cancelled event',()=>{
 const out=summary([row('sos',2,8,earlier-99999999),row('sos',3,9,earlier),row('cancel',3,9,now)]);
 assert.equal(out.attentionCount,1);assert.equal(out.report?.originId,'2');assert.equal(out.report?.eventId,8);
});
test('summary groups relay duplicates and counts distinct open events not vessels',()=>{
 const records=[row('sos',2,9,earlier,3),row('sos',2,9,now,2),row('sos',2,10,now,2),row('sos',4,9,null,3)];
 const reports=emergencyReports(now,records,[]),out=summarizeEmergencyReports(reports);
 assert.equal(out.attentionCount,3);assert.equal(out.report?.eventId,10);assert.equal(out.unknownTimeCount,1);
 assert.equal(reports.find(g=>g.originId==='2'&&g.eventId===9)?.observedAt,earlier);
});
test('shared marker and summary state use origin even when origin has no GPS or device row',()=>{
 const reports=emergencyReports(now,[row('sos',9,1,null,3)],[]);
 assert.equal(popupEmergencyFromReports('9',reports).status,'Needs attention');
 assert.equal(popupEmergencyFromReports('3',reports).status,'No SOS recorded');
 assert.equal(summarizeEmergencyReports(reports).report?.originId,'9');
});
test('matching cancellation clears only corresponding attention and does not require known time',()=>{
 const reports=emergencyReports(now,[row('cancel',2,9,null,3),row('sos',2,9,earlier,2),row('sos',3,9,earlier,2)],[]);
 assert.equal(popupEmergencyFromReports('2',reports).status,'Cancellation observed');
 assert.equal(popupEmergencyFromReports('3',reports).status,'Needs attention');
 assert.equal(summarizeEmergencyReports(reports).attentionCount,1);
});
test('cancel-only reports never create attention and use cancellation observation time',()=>{
 const out=summary([row('cancel',3,9,earlier)]);
 assert.equal(out.attentionCount,0);assert.equal(out.report?.status,'Cancellation only');assert.equal(reportTime(out.report!),earlier);
});
test('unknown-time outstanding reports remain visible without invented time',()=>{
 const out=summary([row('sos',2,9,null),row('sos',3,10,now+5000)]);
 assert.equal(out.attentionCount,2);assert.equal(out.unknownTimeCount,2);assert.equal(out.report?.observedAt,null);
});
test('summary order is deterministic with equal and unknown times',()=>{
 const records=[row('sos',3,1,earlier),row('sos',2,2,earlier),row('sos',2,1,null)];
 assert.deepEqual(summary(records),summary([...records].reverse()));assert.equal(summary(records).report?.observedAt,earlier);
});
test('summary selects most recent known historical event when no attention remains',()=>{
 const out=summary([row('sos',2,1,earlier-5000),row('cancel',2,1,now),row('sos',3,2,earlier),row('cancel',3,2,now)]);
 assert.equal(out.attentionCount,0);assert.equal(out.report?.originId,'3');assert.equal(out.report?.status,'Cancellation observed');
});
test('empty hosted summary remains empty even with fictional input',()=>{
 const samples=[{deviceId:'VESSEL-B',timestamp:earlier,latitude:8,longitude:124}];
 assert.equal(summarizeEmergencyReports(emergencyReports(now,[],samples)).report,null);
 assert.equal(summarizeEmergencyReports(emergencyReports(now,undefined,samples)).attentionCount,1);
});
test('invalid or future demo timestamps do not create attention markers or reports',()=>{
 const samples=[{deviceId:'VESSEL-B',timestamp:now+1,latitude:8,longitude:124},{deviceId:'VESSEL-B',timestamp:NaN,latitude:8,longitude:124}];
 assert.deepEqual(emergencyReports(now,undefined,samples),[]);
});
