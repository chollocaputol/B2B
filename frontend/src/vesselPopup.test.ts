import { test } from 'node:test';
import assert from 'node:assert/strict';
import { popupEmergency, popupBattery } from './vesselPopup.ts';
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
