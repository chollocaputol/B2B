import test from 'node:test';
import assert from 'node:assert/strict';
import {studyDashboard,describeRecord,studyCsv,excludeKnownFixtures,orderStudyHistory} from './studyDashboardData.ts';
import type {Row} from './studySf.ts';
const now=Date.parse('2026-11-25T03:00:00Z'), t='2026-11-25T02:00:00Z', t2='2026-11-25T02:01:00Z';
const row=(n:number,kind:Row['record_json']['kind'],payload:Record<string,unknown>,at:string|null=t,boot='a',node=3):Row=>({recorder_node_id:node,record_id:`${boot}:${n}`,synced_at:'2026-11-25T02:30:00Z',record_json:{recorder_node_id:node,record_id:`${boot}:${n}`,kind,payload,captured_at:at,uptime_ms:n}});
const w={temperature_c:29,humidity_percent:78,pressure_hpa:1009,pressure_trend_hpa:-1,weather_state:1};const g={gps_fix:true,latitude:8.31,longitude:124.2};
test('separate weather and GPS observations retain respective latest values',()=>{const p=studyDashboard([row(1,'weather',w),row(2,'gps',g,t2)],now);assert.equal(p.latestWeather['3']?.temperature,29);assert.equal(p.positions[0].latitude,8.31);assert.equal(p.readings.length,1);assert.equal(p.positions[0].temperature,null);});
test('missing RSSI, battery and shore distance remain null',()=>{const p=studyDashboard([row(1,'weather',w),row(2,'gps',g)],now);for(const a of [...p.readings,...p.positions]){assert.equal(a.signal,null);assert.equal(a.battery,null);assert.equal(a.shoreDistance,null);}});
test('unknown and future capture times retained only in history',()=>{const p=studyDashboard([row(1,'weather',w,null),row(2,'gps',g,null),row(3,'weather',w,'2026-11-26T00:00:00Z')],now);assert.equal(p.records.length,3);assert.equal(p.readings.length,0);assert.equal(p.positions.length,0);assert.equal(p.latestWeather['3'],undefined);});
test('cross-boot equal latest timestamp stays ambiguous with chart gap',()=>{const p=studyDashboard([row(1,'weather',w),row(1,'weather',w,t,'b'),row(2,'gps',g),row(2,'gps',g,t,'b')],now);assert.equal(p.latestWeather['3'],undefined);assert.equal(p.readings[0].temperature,null);assert.equal(p.positions.length,0);assert.equal(p.records.length,4);});
test('same boot tie uses numeric counter; no-fix does not erase last valid GPS',()=>{const p=studyDashboard([row(9,'weather',w),row(10,'weather',{...w,temperature_c:30}),row(11,'gps',g),row(12,'gps',{gps_fix:false,latitude:null,longitude:null},t2)],now);assert.equal(p.latestWeather['3']?.temperature,30);assert.equal(p.positions[0].latitude,8.31);});
test('invalid latitude and missing fix do not create markers',()=>{assert.equal(studyDashboard([row(1,'gps',{...g,latitude:91}),row(2,'gps',{...g,gps_fix:false})],now).positions.length,0);});
test('recording vessel identities stay separate; emergency cancellation dominates globally',()=>{const p=studyDashboard([row(1,'weather',w),row(1,'weather',{...w,temperature_c:20},t,'a',4),row(2,'cancel',{origin_node_id:2,event_id:1}),row(3,'sos',{origin_node_id:2,event_id:1},null,'a',4)],now);assert.deepEqual(p.devices.map(d=>d.id),['3','4']);assert.equal(p.latestWeather['4']?.temperature,20);assert.equal(p.events[0].state,'Cancellation observed');assert.equal(p.sosEvents.length,0);});
test('CSV preserves record identity, unknown capture, receipt and exact payload',()=>{const r=row(1,'gps',g,null);const csv=studyCsv([r]);assert.equal(csv.split('\n').length,2);assert.ok(csv.includes('"gps","","2026-11-25T02:30:00Z"'));assert.ok(csv.includes('""gps_fix"":true'));assert.equal(describeRecord(r),'GPS fix: 8.31, 124.2');});
test('empty snapshot has no invented vessels and duplicate snapshot fails closed',()=>{assert.equal(studyDashboard([],now).devices.length,0);const r=row(1,'weather',w);assert.throws(()=>studyDashboard([r,r],now));});

test('known fixture identities excluded exactly, including node and counter bounds',()=>{
 const a='aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa',b='bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb';
 const fixtures=[...Array.from({length:40},(_,i)=>row(i+1,'sos',{origin_node_id:2,event_id:1},null,a)),...Array.from({length:8},(_,i)=>row(i+1,'gps',g,null,b))];
 const other=[row(41,'weather',w,t,a),row(9,'gps',g,t,b),row(1,'gps',g,t,a,4),row(1,'sos',{origin_node_id:3,event_id:23})];
 const input=[...fixtures,...other],p=excludeKnownFixtures(input);
 assert.equal(p.excluded,48);assert.deepEqual(p.records,other);assert.equal(input.length,52);assert.equal(studyDashboard(p.records,now).events.length,1);
 assert.equal(excludeKnownFixtures(fixtures).records.length,0);
 assert.throws(()=>excludeKnownFixtures([fixtures[0],fixtures[0]]));
});
test('history orders known capture newest first and preserves unknown times and input',()=>{
 const unknown=row(3,'gps',g,null),older=row(1,'weather',w,t),newer=row(2,'gps',g,t2);
 const unknown2={...row(4,'weather',w,null),synced_at:'2026-11-25T02:31:00Z'};
 const input=[unknown,older,unknown2,newer];
 assert.deepEqual(orderStudyHistory(input),[newer,older,unknown2,unknown]);
 assert.deepEqual(input,[unknown,older,unknown2,newer]);assert.equal(unknown.record_json.captured_at,null);
});
