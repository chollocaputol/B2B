import { useEffect, useLayoutEffect, useRef, useState } from 'react';
import type {SnapshotPhase} from './refreshState';
import L from 'leaflet';
import 'leaflet/dist/leaflet.css';
import { studyAccessMode, externalBasemapEnabled } from './studyAccessMode';
import { LocateFixed, MapPin, Navigation } from 'lucide-react';
import { ageLabel, getLastPosition, type Device, type Reading, type SosEvent } from './data';

// In-memory view state owned by the authenticated dashboard, never browser storage.
export type MapSession = { viewport?: {center:[number,number];zoom:number}; selectedId?:string; tilesOptIn?:boolean };

type Props = { devices: Device[]; readings: Reading[]; sosEvents: SosEvent[]; selectedId: string; now: number; onSelect: (id: string) => void; synchronized?:boolean; mapSession?:MapSession; studyPhase?:SnapshotPhase };
export default function VesselMap({ devices, readings, sosEvents, selectedId, now, onSelect, synchronized=false, mapSession, studyPhase='ready' }: Props) {
 const container = useRef<HTMLDivElement>(null);
 const map = useRef<L.Map | null>(null);
 const markers = useRef<Record<string,L.Marker>>({});
 const [tileError,setTileError] = useState(false);
 const [tilesOptIn,setTilesOptIn] = useState(()=>mapSession?.tilesOptIn??false);
 const mode = studyAccessMode(location.hostname,location.protocol,import.meta.env.VITE_SUPABASE_URL);
 const showTileControl = synchronized && mode === 'hosted';
 const showTiles = externalBasemapEnabled(synchronized,mode,tilesOptIn);
 useLayoutEffect(() => {
  if (!container.current) return;
  const initialFix = getLastPosition(readings,selectedId,now);
  const center: L.LatLngExpression = mapSession?.viewport?.center ?? (initialFix ? [initialFix.latitude!,initialFix.longitude!] : [8.283,124.245]);
  const instance = L.map(container.current,{scrollWheelZoom:false,maxZoom:18}).setView(center,mapSession?.viewport?.zoom ?? 13);
  map.current = instance;
  const rememberView = () => {
   if (!mapSession) return;
   const c = instance.getCenter();
   mapSession.viewport = {center:[c.lat,c.lng],zoom:instance.getZoom()};
  };
  rememberView();
  instance.on('moveend',rememberView);
  const observer = new ResizeObserver(() => instance.invalidateSize({animate:false,pan:true}));
  observer.observe(container.current);
  return () => { observer.disconnect(); instance.off('moveend',rememberView); instance.remove(); map.current=null; markers.current={}; };
 // Only a different dashboard session may replace the map instance.
 },[mapSession]);
 useLayoutEffect(() => {
  const instance = map.current;
  if (!instance) return;
  const observations = L.layerGroup().addTo(instance);
  const reference = document.createElement('span'); reference.textContent=synchronized?'Acmac · designated sync shore':'Acmac · area reference';
  L.circleMarker(synchronized?[8.275292,124.262623]:[8.274,124.2649],{radius:5,color:'#526c7e',fillColor:'#fff',fillOpacity:1,weight:2}).bindTooltip(reference,{permanent:true,direction:'right',className:'shore-label'}).addTo(observations);
  for (const device of devices) {
   const position = getLastPosition(readings,device.id,now);
   if (!position) continue;
   const icon = document.createElement('span'); icon.className='boat-pin'; icon.textContent=synchronized?device.id:device.name.replace('Vessel ','');
   const tooltip = document.createElement('span'); tooltip.textContent=`${device.name} · ${synchronized?'last timed GPS fix':'sample position'}`;
   const marker = L.marker([position.latitude!,position.longitude!],{
    icon:L.divIcon({className:'vessel-marker',html:icon,iconSize:[44,44],iconAnchor:[22,22]}), title:`Select ${device.name} ${synchronized?'last timed GPS fix':'sample position'}`, alt:`${device.name} last reported ${synchronized?'GPS fix':'sample GPS position'}`, keyboard:true,
   }).addTo(observations).bindTooltip(tooltip,{direction:'top',offset:[0,-12]});
   marker.getElement()?.setAttribute('aria-label',`Select ${device.name} ${synchronized?'last timed GPS fix':'sample position'}`);
   marker.on('click',() => onSelect(device.id)); markers.current[device.id]=marker;
  }
  for (const event of sosEvents) {
   const label = document.createElement('span'); label.textContent=`Sample SOS · ${event.deviceId} · event location, not current position`;
   L.circleMarker([event.latitude,event.longitude],{radius:18,color:'#d07153',fillColor:'#f3ad89',fillOpacity:0.15,weight:2,dashArray:'4 4'}).bindTooltip(label).addTo(observations);
  }
  return () => { if (instance.hasLayer(observations)) instance.removeLayer(observations); markers.current={}; };
 },[devices,readings,sosEvents,now,onSelect,synchronized,mapSession]);
 useLayoutEffect(() => {
  Object.entries(markers.current).forEach(([id,marker]) => marker.getElement()?.classList.toggle('is-selected',id===selectedId));
  // Selection changes highlight only. Explicit map reset remains available.
 },[selectedId,devices,readings,sosEvents,now,onSelect,synchronized,mapSession]);
 // The basemap is optional; do not rebuild the marker map when the toggle changes.
 // Change tiles independently of observation and selection updates.
 useEffect(() => {
  setTileError(false);
  const instance = map.current;
  if (!instance || !showTiles) return;
  const layer = L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
   maxZoom:18, attribution:'&copy; <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noopener noreferrer">OpenStreetMap contributors</a>',
  });
  const onError = () => setTileError(true);
  layer.on('tileerror',onError);
  layer.addTo(instance);
  return () => { layer.off('tileerror',onError); if (instance.hasLayer(layer)) instance.removeLayer(layer); };
 },[showTiles,mapSession]);
 const selected = getLastPosition(readings,selectedId,now);
 return <section className="panel vessel-map-panel" id="vessel-map">
  <div className="panel-heading"><div><h2><Navigation size={16}/> Vessel locations <span className="live-label">{synchronized?'LAST TIMED FIX':'SAMPLE POSITIONS'}</span></h2><p>Iligan Bay · Barangay Acmac, Iligan City</p></div><button className="map-reset" onClick={() => map.current?.setView([8.283,124.245],13)} aria-label="Reset map to Acmac area"><LocateFixed size={16}/></button></div>
  {showTileControl&&<div className="map-error"><label><input type="checkbox" checked={tilesOptIn} onChange={e=>{const checked=e.target.checked;setTilesOptIn(checked);if(mapSession)mapSession.tilesOptIn=checked;}}/> Show external basemap</label><div>Map requests reveal the viewed area and IP address to OpenStreetMap. Enable only if that area may be shared; leave off for confidential locations. Turning off stops new tile requests.</div></div>}
  <div className="map-container" ref={container} role="region" aria-label={synchronized?'Last timed synchronized vessel GPS fixes':'Interactive map of fictional sample vessel positions near Acmac'}/>
  {showTiles&&tileError&&<div className="map-error" role="status">Some basemap tiles could not load. Vessel markers and available coordinates remain visible.</div>}
  <div className="map-location"><MapPin size={15}/><div><strong>{devices.find(d => d.id===selectedId)?.name??'Vessel positions'} · {studyPhase==='loading'?'Loading':studyPhase==='error'?'Unavailable':selected?'last reported GPS fix':'GPS unavailable'}</strong><span>{studyPhase==='loading'?'Refreshing synchronized vessel positions…':studyPhase==='error'?'Unable to load vessel positions.':studyPhase==='empty'?'No synchronized vessel positions available.':selected?`${selected.latitude!.toFixed(5)}° N, ${selected.longitude!.toFixed(5)}° E · ${ageLabel(selected.timestamp,now)}`:synchronized?'No unambiguous timed valid fix. No marker is shown for this vessel.':'No valid coordinates received. No marker is shown for this vessel.'}</span></div></div>
  <div className="map-caption"><span><i className="legend-vessel"/> {synchronized?'Last timed vessel fix':'Sample vessel'} {!synchronized&&<><i className="legend-sos"/> Sample SOS event location</>}</span><small>{synchronized?(showTiles?'External basemap enabled':showTileControl?'External basemap off':'Local review: external basemap disabled'):'Illustrative positions only'}</small></div>
 </section>;
}
