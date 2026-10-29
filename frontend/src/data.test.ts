import { test } from 'node:test';
import assert from 'node:assert/strict';
import { filterReadings, getDeviceStatus, getDemoData, getLatestReading, getLastPosition, getVesselSnapshot, weatherLabel, type Reading } from './data.ts';
const now = Date.parse('2026-10-29T04:00:00Z');
const row = (deviceId: string, hoursAgo: number): Reading => ({ deviceId, timestamp: now - hoursAgo * 3600000, temperature: 25, humidity: 60, signal: -55, pressure:1010, pressureTrend:0, latitude:null, longitude:null, gpsFix:false, battery:80, shoreDistance:0, weatherState:0 });
test('range includes start, excludes future, isolates devices and sorts chronologically', () => {
 const result = filterReadings([row('a', 0), row('b', 1), row('a', 25), row('a', 24), row('a', -1)], 'a', 24, now);
 assert.deepEqual(result.map(r => r.timestamp), [now - 86400000, now]);
});
test('unknown device returns empty readings', () => assert.deepEqual(filterReadings([row('a', 0)], 'missing', 24, now), []));
test('device status uses latest valid reading and handles missing, future and stale devices', () => {
 assert.equal(getDeviceStatus([row('a', 1), row('a', 0)], 'a', now), 'online');
 assert.equal(getDeviceStatus([row('a', 1)], 'a', now), 'offline');
 assert.equal(getDeviceStatus([], 'a', now), 'offline');
 assert.equal(getDeviceStatus([row('a', -1)], 'a', now), 'offline');
});
test('demo data is deterministic with a current device and a stale device', () => {
 const first = getDemoData(now);
 assert.deepEqual(first, getDemoData(now));
 assert.equal(first.devices.length, 3);
 assert.equal(getDeviceStatus(first.readings, first.devices[0].id, now), 'online');
 assert.equal(getDeviceStatus(first.readings, first.devices[2].id, now), 'offline');
 assert.ok(filterReadings(first.readings, first.devices[0].id, 168, now).length > filterReadings(first.readings, first.devices[0].id, 24, now).length);
});

test('latest packet is independent of history ordering and ignores future records', () => {
 assert.equal(getLatestReading([row('a',0),row('a',2),row('b',0),row('a',-1)],'a',now)?.timestamp,now);
 assert.equal(getLatestReading([],'a',now),null);
});
test('last position ignores missing fixes and out-of-range coordinates', () => {
 const valid = {...row('a',1),gpsFix:true,latitude:8.28,longitude:124.24};
 const invalid = {...row('a',0),gpsFix:false,latitude:null,longitude:null};
 assert.equal(getLastPosition([invalid,valid],'a',now)?.timestamp,now-3600000);
 assert.equal(getLastPosition([invalid,{...valid,latitude:91}],'a',now),null);
 assert.equal(getLastPosition([{...valid,longitude:181}],'a',now),null);
});
test('weather states follow the firmware contract without equating normal with safe', () => {
 assert.equal(weatherLabel(0),'Calibrating');
 assert.equal(weatherLabel(1),'Normal readings');
 assert.equal(weatherLabel(2),'Approaching front');
 assert.equal(weatherLabel(3),'Immediate squall');
 assert.equal(weatherLabel(99),'Unavailable');
});
test('marine samples include pressure and retain unavailable sensor readings', () => {
 const {readings,devices} = getDemoData(now);
 const vesselA = getLatestReading(readings,devices[0].id,now)!;
 const vesselB = getLatestReading(readings,devices[1].id,now)!;
 assert.ok(vesselA.pressure !== null && vesselA.pressure > 900 && vesselA.pressure < 1100);
 assert.equal(vesselB.pressure,null);
 assert.equal(vesselB.temperature,null);
 assert.equal(vesselB.weatherState,0);
});

test('shore distance stays with the last valid GPS report after a new packet loses its fix', () => {
 const valid = {...row('a',1),gpsFix:true,latitude:8.28,longitude:124.24,shoreDistance:2800};
 const noFix = {...row('a',0),gpsFix:false,latitude:null,longitude:null,shoreDistance:0};
 const snapshot = getVesselSnapshot([valid,noFix],'a',now);
 assert.equal(snapshot.latest?.timestamp,now);
 assert.equal(snapshot.position?.timestamp,now-3600000);
 assert.equal(snapshot.shoreDistance,2800);
 assert.equal(getVesselSnapshot([noFix],'a',now).shoreDistance,null);
 assert.equal(getVesselSnapshot([{...valid,shoreDistance:-1}],'a',now).shoreDistance,null);
});
