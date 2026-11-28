import { useEffect, useRef } from 'react';
import { ArrowRight, Radio, X } from 'lucide-react';
export default function InfoDialog({ onClose, study = false, basemapEnabled, onBasemapChange }: { onClose: () => void; study?: boolean; basemapEnabled?: boolean; onBasemapChange?: (enabled: boolean) => void }) {
    const ref = useRef<HTMLDialogElement>(null);
    useEffect(() => {
        const opener = document.activeElement instanceof HTMLElement ? document.activeElement : null;
        const dialog = ref.current!;
        dialog.showModal();
        return () => { dialog.close(); opener?.focus(); };
    }, []);
    return <dialog ref={ref} className="help-dialog" aria-labelledby="help-title" onCancel={onClose} onClick={e => { if (e.target === e.currentTarget) onClose() }}>
        <button className="dialog-close icon-button" aria-label="Close dashboard information" onClick={onClose}><X size={20} /></button>
        <span className="brand-icon"><Radio size={24} /></span><h2 id="help-title">For the Acmac fishing community.</h2>
        <p>This research dashboard serves small-scale fishermen and shore monitoring in Barangay Acmac, Iligan City, Lanao del Norte. {study ? 'This study view shows authenticated synchronized observations, excluding the known synthetic fixture identities.' : 'All vessels, positions, weather readings and SOS events are fictional samples.'}</p>
        <p>{study ? 'Delayed synchronization does not establish live LoRa connectivity.' : 'Online and offline labels describe the captured demo snapshot.'} GPS positions are last reported fixes, never a guarantee of current location. All times use Philippine Standard Time (UTC+8).</p>
        <p>{study ? 'This is synchronized history, not a live hardware feed.' : 'No live hardware or Supabase connection is active.'} The website cannot send SOS messages, dispatch rescue, or provide official weather forecasts. {!study && 'Vessel A models a sensor node; Vessel B models a GPS/LoRa node with unavailable local weather sensors.'}</p>
        {onBasemapChange && <section aria-label="External basemap privacy">
            <h3>Map privacy</h3>
            <p><label><input type="checkbox" checked={basemapEnabled === true} onChange={e => onBasemapChange(e.target.checked)} /> Show external basemap</label></p>
            <p>Enabled by default for authorized hosted access. Tile requests share the viewed area and your IP address with OpenStreetMap. Turn off for confidential locations; this stops new tile requests for this dashboard session. Previously requested tiles cannot be recalled.</p>
        </section>}
        <button className="button primary-button" onClick={onClose}>Back to the dashboard <ArrowRight size={16} /></button>
    </dialog>;
}
