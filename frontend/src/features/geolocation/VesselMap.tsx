import { useEffect, useLayoutEffect, useRef, useState } from 'react';
import type { SnapshotPhase } from '../../lib/refreshState';
import L from 'leaflet';
import 'leaflet/dist/leaflet.css';
import { studyAccessMode, externalBasemapEnabled } from '../auth/studyAccessMode';
import { LocateFixed, MapPin, Navigation } from 'lucide-react';
import { ageLabel, dateTime, getLastPosition, getLatestReading, weatherLabel, type Device, type Reading, type SosEvent } from '../../lib/data';
import type { studyDashboard } from '../../lib/studyDashboardData';
import { batteryDetail, type RecordedBattery } from '../weather/wxbatPresentation';
import { popupEmergencyFromReports, emergencyReports, popupBattery, type PopupEmergency } from './vesselPopup';
import { SHORE, shoreReference, distanceToShore, distanceText, distanceLayout } from './geolocation';

// In-memory view state owned by the authenticated dashboard, never browser storage.
export type MapSession = { viewport?: { center: [number, number]; zoom: number }; selectedId?: string; tilesEnabled?: boolean };

type OpenInfo = { id: string; layer: L.Tooltip; marker: L.Marker };
function dismissInfo(instance: L.Map | null, active: { current: OpenInfo | null }, restoreFocus = false) {
    const info = active.current;
    if (!info) return;
    active.current = null;
    if (instance?.hasLayer(info.layer)) instance.removeLayer(info.layer);
    const element = info.marker.getElement();
    element?.setAttribute('aria-expanded', 'false');
    if (restoreFocus) element?.focus({ preventScroll: true });
}
function popupContent(device: Device, position: Reading, weather: Reading | null | undefined, sos: PopupEmergency, synchronized: boolean, battery: RecordedBattery | undefined, now: number, onClose: () => void) {
    const card = document.createElement('div'); card.className = 'vessel-info-card';
    const heading = document.createElement('div'); heading.className = 'vessel-info-heading';
    const title = document.createElement('strong'); title.textContent = device.name;
    const close = document.createElement('button'); close.type = 'button'; close.className = 'vessel-info-close'; close.textContent = '×'; close.setAttribute('aria-label', 'Close vessel information'); close.addEventListener('click', onClose);
    heading.append(title, close); card.append(heading);
    const list = document.createElement('dl');
    const attention = sos.status === 'Needs attention';
    const fields = [['Vessel name', device.registeredName ?? (synchronized ? 'Unavailable' : device.name)], ['Registration no.', device.registrationSerialNo ?? 'Unavailable'], ['SOS status', sos.status], ['Weather state', weatherLabel(weather?.weatherState ?? -1)], [synchronized ? 'Battery estimate' : 'Battery', synchronized ? batteryDetail(battery, now) : popupBattery(weather)], [attention ? 'SOS observed at' : 'Last GPS fix', attention ? (sos.observedAt === null ? 'Unknown' : dateTime(sos.observedAt) + ' PHT') : dateTime(position.timestamp) + ' PHT']];
    for (const [label, value] of fields) { const term = document.createElement('dt'); term.textContent = label; const detail = document.createElement('dd'); detail.textContent = value; list.append(term, detail); }
    card.append(list);
    const note = document.createElement('small'); note.textContent = synchronized ? (attention ? 'Recorded SOS observation; marker is last GPS fix, not distress location.' : 'Synchronized observations; not live position.') : 'Fictional sample observations.'; card.append(note);
    L.DomEvent.disableClickPropagation(card); L.DomEvent.disableScrollPropagation(card);
    return { card, close };
}

type Props = { study?: ReturnType<typeof studyDashboard>; devices: Device[]; readings: Reading[]; sosEvents: SosEvent[]; selectedId: string; now: number; onSelect: (id: string) => void; synchronized?: boolean; mapSession?: MapSession; tilesEnabled: boolean; studyPhase?: SnapshotPhase };
export default function VesselMap({ study, devices, readings, sosEvents, selectedId, now, onSelect, synchronized = false, mapSession, tilesEnabled, studyPhase = 'ready' }: Props) {
    const container = useRef<HTMLDivElement>(null);
    const map = useRef<L.Map | null>(null);
    const markers = useRef<Record<string, L.Marker>>({});
    const activeInfo = useRef<OpenInfo | null>(null);
    const clearTapForSelection = useRef<(id: string) => void>(() => { });
    const [tileError, setTileError] = useState(false);
    const mode = studyAccessMode(location.hostname, location.protocol, import.meta.env.VITE_SUPABASE_URL);
    const showTiles = externalBasemapEnabled(synchronized, mode, tilesEnabled);
    useLayoutEffect(() => {
        if (!container.current) return;
        const initialFix = getLastPosition(readings, selectedId, now);
        const center: L.LatLngExpression = mapSession?.viewport?.center ?? (initialFix ? [initialFix.latitude!, initialFix.longitude!] : [8.283, 124.245]);
        const instance = L.map(container.current, { scrollWheelZoom: false, maxZoom: 18 }).setView(center, mapSession?.viewport?.zoom ?? 13);
        map.current = instance;
        const rememberView = () => {
            if (!mapSession) return;
            const c = instance.getCenter();
            mapSession.viewport = { center: [c.lat, c.lng], zoom: instance.getZoom() };
        };
        rememberView();
        instance.on('moveend', rememberView);
        const observer = new ResizeObserver(() => instance.invalidateSize({ animate: false, pan: true }));
        observer.observe(container.current);
        return () => { dismissInfo(instance, activeInfo); observer.disconnect(); instance.off('moveend', rememberView); instance.remove(); map.current = null; markers.current = {}; };
        // Only a different dashboard session may replace the map instance.
    }, [mapSession]);
    useLayoutEffect(() => {
        const instance = map.current;
        if (!instance) return;
        const observations = L.layerGroup().addTo(instance);
        const distanceLayer = L.layerGroup().addTo(observations);
        const shore = shoreReference(synchronized), shorePoint = L.latLng(shore.latitude, shore.longitude);
        type DistanceTarget = { id: string; point: L.LatLng; metres: number; attention: boolean };
        let hovered: DistanceTarget | null = null, focused: DistanceTarget | null = null, tapped: DistanceTarget | null = null, touchInteraction = false;
        const redrawDistance = () => {
            distanceLayer.clearLayers();
            const target = tapped ?? hovered ?? focused;
            if (!target) return;
            const label = distanceText(target.metres), width = Math.max(48, label.length * 8 + 10);
            const layout = distanceLayout(instance.latLngToLayerPoint(target.point), instance.latLngToLayerPoint(shorePoint), width);
            const color = target.attention ? '#b91c1c' : '#385665';
            const asLatLng = (p: { x: number; y: number }) => instance.layerPointToLatLng(L.point(p.x, p.y));
            for (const segment of layout.segments) L.polyline(segment.map(asLatLng), { color, weight: 2, dashArray: '5 5', interactive: false }).addTo(distanceLayer);
            const text = document.createElement('span'); text.textContent = label;
            L.marker(asLatLng(layout.center), { icon: L.divIcon({ className: 'shore-distance-label' + (target.attention ? ' distance-sos' : ''), html: text, iconSize: [width, 20], iconAnchor: [width / 2, 10] }), interactive: false, keyboard: false, zIndexOffset: -1000 }).addTo(distanceLayer);
        };
        const dismiss = () => { tapped = null; dismissInfo(instance, activeInfo); redrawDistance(); };
        const keyboardInteraction = (event: KeyboardEvent) => { if (event.key === 'Tab' || event.key === 'Enter' || event.key === ' ') touchInteraction = false; };
        clearTapForSelection.current = (id: string) => { if (tapped && tapped.id !== id) { tapped = null; redrawDistance(); } };
        instance.on('moveend zoomend resize', redrawDistance);
        const onEscape = (event: KeyboardEvent) => { keyboardInteraction(event); if (event.key === 'Escape' && activeInfo.current) { event.preventDefault(); event.stopPropagation(); tapped = null; dismissInfo(instance, activeInfo, true); redrawDistance(); } };
        const host = instance.getContainer();
        instance.on('click', dismiss); host.addEventListener('keydown', onEscape, true);
        const reference = document.createElement('span'); reference.textContent = synchronized ? 'Acmac · designated sync shore' : 'Acmac · area reference';
        if (synchronized) {
            L.circle(shorePoint, { radius: SHORE.exitM, color: '#617b85', weight: 1.5, dashArray: '5 4', fill: false, interactive: false }).addTo(observations);
            L.circle(shorePoint, { radius: SHORE.enterM, color: '#16877e', weight: 1.5, fill: false, interactive: false }).addTo(observations);
        }
        L.circleMarker(shorePoint, { radius: 5, color: '#526c7e', fillColor: '#fff', fillOpacity: 1, weight: 2 }).bindTooltip(reference, { permanent: true, direction: 'right', className: 'shore-label' }).addTo(observations);
        const reports = studyPhase === 'ready' && (!synchronized || study) ? emergencyReports(now, synchronized ? (study?.records ?? []) : undefined, sosEvents) : [];
        for (const device of devices) {
            const position = getLastPosition(readings, device.id, now);
            if (!position) continue;
            const sos = popupEmergencyFromReports(device.id, reports);
            const attention = sos.status === 'Needs attention';
            const icon = document.createElement('span'); icon.className = 'boat-pin'; icon.textContent = synchronized ? device.id : device.name.replace('Vessel ', '');
            const marker = L.marker([position.latitude!, position.longitude!], {
                icon: L.divIcon({ className: attention ? 'vessel-marker has-sos' : 'vessel-marker', html: icon, iconSize: [44, 44], iconAnchor: [22, 22] }), alt: `${device.name} last reported ${synchronized ? 'GPS fix' : 'sample GPS position'}`, keyboard: true, zIndexOffset: attention ? 1000 : 0,
            }).addTo(observations);
            marker.getElement()?.setAttribute('aria-label', `Select ${device.name} ${synchronized ? 'last timed GPS fix' : 'sample position'}${attention ? ' · SOS needs attention' : ''}`);
            const elementForDistance = marker.getElement(), metres = distanceToShore(position, synchronized);
            const target = metres === null ? null : { id: device.id, point: marker.getLatLng(), metres, attention };
            if (elementForDistance && target) {
                elementForDistance.setAttribute('aria-label', elementForDistance.getAttribute('aria-label') + ' · ' + distanceText(metres) + ' to shore reference at last fix');
                elementForDistance.addEventListener('pointerdown', e => { touchInteraction = e.pointerType === 'touch'; if (touchInteraction) { hovered = null; focused = null; redrawDistance(); } });
                elementForDistance.addEventListener('pointerenter', e => { if (e.pointerType !== 'touch') { hovered = target; redrawDistance(); } });
                elementForDistance.addEventListener('pointerleave', () => { if (hovered?.id === device.id) hovered = null; redrawDistance(); });
                elementForDistance.addEventListener('focus', () => { if (elementForDistance.matches(':focus-visible')) { touchInteraction = false; focused = target; redrawDistance(); } });
                elementForDistance.addEventListener('blur', () => { if (focused?.id === device.id) focused = null; redrawDistance(); });
            }
            marker.getElement()?.setAttribute('aria-haspopup', 'dialog');
            marker.getElement()?.setAttribute('aria-expanded', 'false');
            marker.on('click', () => {
                onSelect(device.id);
                dismiss();
                if (studyPhase !== 'ready' || (synchronized && !study)) return;
                // Hosted GPS rows and weather rows are intentionally separate projections.
                const weather = synchronized ? study?.latestWeather[device.id] : getLatestReading(readings, device.id, now);
                const { card, close } = popupContent(device, position, weather, sos, synchronized, study?.latestBattery[device.id], now, () => { tapped = null; dismissInfo(instance, activeInfo, true); redrawDistance(); });
                if (touchInteraction) { tapped = target; redrawDistance(); }
                card.style.width = Math.min(246, Math.max(120, Math.floor((instance.getSize().x - 40) / 2))) + 'px';
                const layer = L.tooltip({ direction: 'auto', offset: [14, 0], permanent: true, interactive: true, opacity: 1, className: 'vessel-info-popup' }).setLatLng(marker.getLatLng()).setContent(card).addTo(instance);
                activeInfo.current = { id: device.id, layer, marker };
                const element = layer.getElement(); element?.setAttribute('role', 'dialog'); element?.setAttribute('aria-label', device.name + ' information');
                marker.getElement()?.setAttribute('aria-expanded', 'true');
                close.focus({ preventScroll: true });
            }); markers.current[device.id] = marker;
        }
        return () => { clearTapForSelection.current = () => { }; hovered = null; focused = null; tapped = null; dismiss(); instance.off('moveend zoomend resize', redrawDistance); instance.off('click', dismiss); host.removeEventListener('keydown', onEscape, true); if (instance.hasLayer(observations)) instance.removeLayer(observations); markers.current = {}; };
    }, [devices, readings, sosEvents, now, onSelect, synchronized, mapSession, study, studyPhase]);
    useLayoutEffect(() => {
        Object.entries(markers.current).forEach(([id, marker]) => marker.getElement()?.classList.toggle('is-selected', id === selectedId));
        clearTapForSelection.current(selectedId);
        if (activeInfo.current && activeInfo.current.id !== selectedId) dismissInfo(map.current, activeInfo);
        // Selection changes highlight only. Explicit map reset remains available.
    }, [selectedId, devices, readings, sosEvents, now, onSelect, synchronized, mapSession, study, studyPhase]);
    // The basemap is optional; do not rebuild the marker map when the toggle changes.
    // Change tiles independently of observation and selection updates.
    useEffect(() => {
        setTileError(false);
        const instance = map.current;
        if (!instance || !showTiles) return;
        const layer = L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
            maxZoom: 18, attribution: '&copy; <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noopener noreferrer">OpenStreetMap contributors</a>',
        });
        const onError = () => setTileError(true);
        layer.on('tileerror', onError);
        layer.addTo(instance);
        return () => { layer.off('tileerror', onError); if (instance.hasLayer(layer)) instance.removeLayer(layer); };
    }, [showTiles, mapSession]);
    const selected = getLastPosition(readings, selectedId, now);
    return <section className="panel vessel-map-panel" id="vessel-map">
        <div className="panel-heading"><div><h2><Navigation size={16} /> Vessel locations <span className="live-label">{synchronized ? 'LAST TIMED FIX' : 'SAMPLE POSITIONS'}</span></h2><p>Iligan Bay · Barangay Acmac, Iligan City</p></div><button className="map-reset" onClick={() => map.current?.setView([8.283, 124.245], 13)} aria-label="Reset map to Acmac area"><LocateFixed size={16} /></button></div>

        <div className="map-container" ref={container} role="region" aria-label={synchronized ? 'Last timed synchronized vessel GPS fixes' : 'Interactive map of fictional sample vessel positions near Acmac'} />
        {showTiles && tileError && <div className="map-error" role="status">Some basemap tiles could not load. Vessel markers and available coordinates remain visible.</div>}
        <div className="map-location"><MapPin size={15} /><div><strong>{devices.find(d => d.id === selectedId)?.name ?? 'Vessel positions'} · {studyPhase === 'loading' ? 'Loading' : studyPhase === 'error' ? 'Unavailable' : selected ? 'last reported GPS fix' : 'GPS unavailable'}</strong><span>{studyPhase === 'loading' ? 'Refreshing synchronized vessel positions…' : studyPhase === 'error' ? 'Unable to load vessel positions.' : studyPhase === 'empty' ? 'No synchronized vessel positions available.' : selected ? `${selected.latitude!.toFixed(5)}° N, ${selected.longitude!.toFixed(5)}° E · ${ageLabel(selected.timestamp, now)}` : synchronized ? 'No unambiguous timed valid fix. No marker is shown for this vessel.' : 'No valid coordinates received. No marker is shown for this vessel.'}</span></div></div>
        {synchronized && <div className="shore-proximity-note"><span><i className="shore-enter-key" />100 m sync entry</span><span><i className="shore-exit-key" />150 m sync exit</span><small>Provisional firmware proximity thresholds, not coverage or legal boundaries. Hover or focus a vessel for straight-line distance to the shore reference.</small></div>}
        <div className="map-caption"><span><i className="legend-vessel" /> {synchronized ? 'Last timed vessel fix' : 'Sample vessel'} <i className="legend-attention" /> {synchronized ? 'SOS needs attention · last GPS fix' : 'Sample SOS · vessel position'}</span><small>{synchronized ? (showTiles ? 'External basemap enabled' : mode === 'hosted' ? 'External basemap off · About' : 'Local review: external basemap disabled') : 'Illustrative positions only'}</small></div>
    </section>;
}
