import { Anchor, BatteryMedium, CloudSun, Compass, Droplets, Gauge, MapPin, Radio, ShieldAlert, Thermometer } from 'lucide-react';
import { ageLabel, dateTime, getVesselSnapshot, weatherLabel, type Device, type Reading, type SosEvent } from './data';
import VesselMap from './VesselMap';

type Props = {devices:Device[];readings:Reading[];sosEvents:SosEvent[];selectedId:string;now:number;onSelect:(id:string)=>void};
export default function MarineOverview(props:Props) {
 const {devices,readings,sosEvents,selectedId,now,onSelect}=props;
 const vessel = devices.find(d=>d.id===selectedId)!;
 const {latest,position,shoreDistance} = getVesselSnapshot(readings,selectedId,now);
 const weather = latest?.weatherState ?? -1;
 const hasWeather = latest?.temperature !== null && latest?.temperature !== undefined;
 return <div className="marine-grid">
  <VesselMap {...props}/>
  <div className="marine-side">
   <section className="panel weather-panel" id="weather"><div className="panel-heading"><div><h2><CloudSun size={17}/> Weather at the vessel</h2><p>{vessel.name} · local sensor sample</p></div><span className="weather-icon"><CloudSun size={30}/></span></div>
    <div className="weather-main"><div><strong>{hasWeather?latest!.temperature!.toFixed(1):'—'}<span>°C</span></strong><p>{hasWeather?'Air temperature':'Weather sensors unavailable'}</p></div><span className={`weather-state state-${weather}`}>{weatherLabel(weather)}</span></div>
    <div className="weather-details"><div><Droplets size={15}/><span>Humidity</span><strong>{latest?.humidity!=null?`${latest.humidity.toFixed(1)} %`:'Unavailable'}</strong></div><div><Gauge size={15}/><span>Pressure</span><strong>{latest?.pressure!=null?`${latest.pressure.toFixed(1)} hPa`:'Unavailable'}</strong></div><div><Thermometer size={15}/><span>Pressure trend</span><strong>{latest?.pressureTrend!=null?`${latest.pressureTrend>0?'+':''}${latest.pressureTrend.toFixed(1)} hPa`:'Unavailable'}</strong></div></div>
    <div className="weather-note">{latest?`Last packet ${ageLabel(latest.timestamp,now)} · `:''}Sensor state only; not an official forecast.</div>
   </section>
   <section className="panel sos-panel" id="alerts"><div className="panel-heading"><div><h2><ShieldAlert size={17}/> SOS activity <span className="scenario-badge">DEMO</span></h2><p>Read-only sample events</p></div></div>
    {sosEvents.map(event=><div className="sos-event" key={`${event.deviceId}-${event.timestamp}`}><div className="sos-event-title"><span className="sos-dot"/><strong>Sample SOS received</strong><span>{ageLabel(event.timestamp,now)}</span></div><p>{devices.find(d=>d.id===event.deviceId)?.name} · {dateTime(event.timestamp)} PHT</p><div className="sos-coordinates"><MapPin size={13}/>{event.latitude.toFixed(5)}° N, {event.longitude.toFixed(5)}° E</div><small>Coordinates at the SOS event; current position may differ.</small><button className="sos-select" onClick={()=>onSelect(event.deviceId)}>View vessel details <Compass size={14}/></button></div>)}
    <p className="sos-footnote">Demonstration only. No distress message or rescue request is being sent.</p>
   </section>
  </div>
  <section className="panel vessel-detail-strip" aria-label="Selected vessel details"><div><Anchor size={18}/><span>Selected vessel<strong>{vessel.name}</strong></span></div><div><MapPin size={17}/><span>Shore distance at last fix<strong title={position?`${ageLabel(position.timestamp,now)} · ${dateTime(position.timestamp)} PHT`:undefined}>{shoreDistance!=null?`${(shoreDistance/1000).toFixed(1)} km`:'Unavailable'}</strong></span></div><div><BatteryMedium size={18}/><span>Battery sample<strong>{latest?`${latest.battery} %`:'Unavailable'}</strong></span></div><div><Radio size={18}/><span>Received LoRa RSSI<strong>{latest?`${latest.signal} dBm`:'Unavailable'}</strong></span></div></section>
 </div>;
}
