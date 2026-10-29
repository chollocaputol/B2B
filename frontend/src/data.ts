export interface Device { id: string; name: string; location: string }
export interface Reading {
 deviceId: string;
 timestamp: number;
 temperature: number | null;
 humidity: number | null;
 pressure: number | null;
 pressureTrend: number | null;
 signal: number;
 latitude: number | null;
 longitude: number | null;
 gpsFix: boolean;
 battery: number;
 shoreDistance: number;
 weatherState: number;
}
export interface SosEvent { deviceId: string; timestamp: number; latitude: number; longitude: number }
export type Metric = 'temperature' | 'humidity' | 'pressure' | 'signal';
export const metricKeys: Metric[] = ['temperature', 'humidity', 'pressure', 'signal'];
export const metrics: Record<Metric, { label: string; unit: string; color: string }> = {
 temperature: { label: 'Temperature', unit: '°C', color: '#198b80' },
 humidity: { label: 'Humidity', unit: '%', color: '#568bd4' },
 pressure: { label: 'Air pressure', unit: 'hPa', color: '#b18b4b' },
 signal: { label: 'LoRa RSSI', unit: 'dBm', color: '#9b7ac7' },
};
export function filterReadings(readings: Reading[], deviceId: string, hours: number, now: number): Reading[] {
 return readings.filter(r => r.deviceId === deviceId && r.timestamp >= now - hours * 3600000 && r.timestamp <= now).sort((a,b) => a.timestamp-b.timestamp);
}
export function getLatestReading(readings: Reading[], deviceId: string, now: number): Reading | null {
 return readings.reduce<Reading | null>((latest, r) => r.deviceId === deviceId && r.timestamp <= now && (!latest || r.timestamp > latest.timestamp) ? r : latest, null);
}
export function getLastPosition(readings: Reading[], deviceId: string, now: number): Reading | null {
 return getLatestReading(readings.filter(r => r.gpsFix && r.latitude !== null && r.longitude !== null && Number.isFinite(r.latitude) && Number.isFinite(r.longitude) && Math.abs(r.latitude) <= 90 && Math.abs(r.longitude) <= 180), deviceId, now);
}
export function getDeviceStatus(readings: Reading[], deviceId: string, now: number): 'online' | 'offline' {
 const latest = getLatestReading(readings,deviceId,now);
 return latest && now-latest.timestamp <= 15*60000 ? 'online' : 'offline';
}
export function weatherLabel(state: number): string {
 return ({0:'Calibrating',1:'Normal readings',2:'Approaching front',3:'Immediate squall'} as Record<number,string>)[state] ?? 'Unavailable';
}
export const dateTime = (time: number) => new Date(time).toLocaleString('en-PH', { timeZone: 'Asia/Manila', month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit' });
export const ageLabel = (time: number, now: number) => {
 const minutes = Math.max(0, Math.floor((now-time)/60000));
 return minutes < 1 ? 'just now' : minutes < 60 ? `${minutes} min ago` : `${Math.floor(minutes/60)} hr ${minutes%60} min ago`;
};
// UI example only. These coordinates do not represent real vessels or a surveyed shore station.
export function getDemoData(now: number): { devices: Device[]; readings: Reading[]; sosEvents: SosEvent[] } {
 const devices = [
  { id: 'VESSEL-A', name: 'Vessel A', location: 'Weather + GPS node · sample' },
  { id: 'VESSEL-B', name: 'Vessel B', location: 'GPS + LoRa receiver · sample' },
  { id: 'DEMO-C', name: 'Demo vessel C', location: 'Offline / no GPS example' },
 ];
 const readings = devices.flatMap((device,d) => Array.from({length: 673},(_,i) => {
  const age = (672-i)*15*60000 + d*2*60000;
  const sensorsAvailable = d !== 1;
  return { deviceId: device.id, timestamp: now-age,
   temperature: sensorsAvailable ? +(29.4+d+Math.sin(i/16+d)*2.1+Math.cos(i/4)*0.35).toFixed(1) : null,
   humidity: sensorsAvailable ? +(75+d*2+Math.cos(i/22+d)*7+Math.sin(i/3)*0.8).toFixed(1) : null,
   pressure: sensorsAvailable ? +(1010.3+Math.sin(i/31+d)*2).toFixed(1) : null,
   pressureTrend: sensorsAvailable ? +(Math.cos(i/31+d)*0.6).toFixed(1) : null,
   signal: Math.round(-91-d*8+Math.sin(i/11)*5),
   latitude: d===2 ? null : +(8.285+d*0.012+Math.sin(i/80+d)*0.002).toFixed(6),
   longitude: d===2 ? null : +(124.235-d*0.008+Math.cos(i/80+d)*0.002).toFixed(6),
   gpsFix: d!==2,
   battery: 86-d*18,
   shoreDistance: d===0 ? 2800 : d===1 ? 4100 : 0,
   weatherState: sensorsAvailable ? 1 : 0,
  };
 }).filter(r => d !== 2 || r.timestamp <= now-3*3600000));
 const sosEvents = [{deviceId:'VESSEL-B', timestamp:now-8*60000, latitude:8.297, longitude:124.227}];
 return { devices, readings, sosEvents };
}

export function getVesselSnapshot(readings: Reading[],deviceId:string,now:number) {
 const latest=getLatestReading(readings,deviceId,now);
 const position=getLastPosition(readings,deviceId,now);
 return {latest,position,shoreDistance:position && Number.isFinite(position.shoreDistance) && position.shoreDistance>=0 ? position.shoreDistance : null};
}
