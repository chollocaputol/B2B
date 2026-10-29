import { useEffect, useRef, useState } from 'react';
import L from 'leaflet';
import 'leaflet/dist/leaflet.css';
import { LocateFixed, MapPin, Navigation } from 'lucide-react';
import { ageLabel, getLastPosition, type Device, type Reading, type SosEvent } from './data';

type Props = { devices: Device[]; readings: Reading[]; sosEvents: SosEvent[]; selectedId: string; now: number; onSelect: (id: string) => void };
export default function VesselMap({ devices, readings, sosEvents, selectedId, now, onSelect }: Props) {
 const container = useRef<HTMLDivElement>(null);
 const map = useRef<L.Map | null>(null);
 const markers = useRef<Record<string,L.Marker>>({});
 const [tileError,setTileError] = useState(false);
 useEffect(() => {
  if (!container.current) return;
  const instance = L.map(container.current,{scrollWheelZoom:false}).setView([8.283,124.245],13);
  map.current = instance;
  L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
   maxZoom:18, attribution:'&copy; <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noopener noreferrer">OpenStreetMap contributors</a>',
  }).on('tileerror',() => setTileError(true)).addTo(instance);
  const reference = document.createElement('span'); reference.textContent='Acmac · area reference';
  L.circleMarker([8.274,124.2649],{radius:5,color:'#526c7e',fillColor:'#fff',fillOpacity:1,weight:2}).bindTooltip(reference,{permanent:true,direction:'right',className:'shore-label'}).addTo(instance);
  for (const device of devices) {
   const position = getLastPosition(readings,device.id,now);
   if (!position) continue;
   const icon = document.createElement('span'); icon.className='boat-pin'; icon.textContent=device.name.replace('Vessel ','');
   const tooltip = document.createElement('span'); tooltip.textContent=`${device.name} · sample position`;
   const marker = L.marker([position.latitude!,position.longitude!],{
    icon:L.divIcon({className:'vessel-marker',html:icon,iconSize:[34,34],iconAnchor:[17,17]}), title:`Select ${device.name} sample position`, alt:`${device.name} last reported sample GPS position`, keyboard:true,
   }).addTo(instance).bindTooltip(tooltip,{direction:'top',offset:[0,-17]});
   marker.getElement()?.setAttribute('aria-label',`Select ${device.name} sample position`);
   marker.on('click',() => onSelect(device.id)); markers.current[device.id]=marker;
  }
  for (const event of sosEvents) {
   const label = document.createElement('span'); label.textContent=`Sample SOS · ${event.deviceId} · event location, not current position`;
   L.circleMarker([event.latitude,event.longitude],{radius:18,color:'#d07153',fillColor:'#f3ad89',fillOpacity:0.15,weight:2,dashArray:'4 4'}).bindTooltip(label).addTo(instance);
  }
  const observer = new ResizeObserver(() => instance.invalidateSize()); observer.observe(container.current);
  return () => { observer.disconnect(); instance.remove(); map.current=null; markers.current={}; };
 },[devices,readings,sosEvents,now,onSelect]);
 useEffect(() => {
  Object.entries(markers.current).forEach(([id,marker]) => marker.getElement()?.classList.toggle('is-selected',id===selectedId));
  const selected = getLastPosition(readings,selectedId,now);
  if (selected) map.current?.panTo([selected.latitude!,selected.longitude!],{animate:false});
 },[selectedId,readings,now]);
 const selected = getLastPosition(readings,selectedId,now);
 return <section className="panel vessel-map-panel" id="vessel-map">
  <div className="panel-heading"><div><h2><Navigation size={16}/> Vessel locations <span className="live-label">SAMPLE POSITIONS</span></h2><p>Iligan Bay · Barangay Acmac, Iligan City</p></div><button className="map-reset" onClick={() => map.current?.setView([8.283,124.245],13)} aria-label="Reset map to Acmac area"><LocateFixed size={16}/></button></div>
  <div className="map-container" ref={container} role="region" aria-label="Interactive map of fictional sample vessel positions near Acmac"/>
  {tileError&&<div className="map-error" role="status">Some basemap tiles could not load. Sample coordinates remain available below.</div>}
  <div className="map-location"><MapPin size={15}/><div><strong>{devices.find(d => d.id===selectedId)?.name} · {selected?'last reported GPS fix':'GPS unavailable'}</strong><span>{selected?`${selected.latitude!.toFixed(5)}° N, ${selected.longitude!.toFixed(5)}° E · ${ageLabel(selected.timestamp,now)}`:'No valid coordinates received. No marker is shown for this vessel.'}</span></div></div>
  <div className="map-caption"><span><i className="legend-vessel"/> Sample vessel <i className="legend-sos"/> Sample SOS event location</span><small>Illustrative positions only</small></div>
 </section>;
}
