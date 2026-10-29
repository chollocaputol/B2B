import { useEffect, useRef } from 'react';
import { ArrowRight, Radio, X } from 'lucide-react';
export default function InfoDialog({ onClose }: { onClose: () => void }) {
 const ref = useRef<HTMLDialogElement>(null);
 useEffect(() => {
  const opener = document.activeElement instanceof HTMLElement ? document.activeElement : null;
  const dialog = ref.current!;
  dialog.showModal();
  return () => { dialog.close(); opener?.focus(); };
 }, []);
 return <dialog ref={ref} className="help-dialog" aria-labelledby="help-title" onCancel={onClose} onClick={e => {if(e.target === e.currentTarget) onClose()}}>
  <button className="dialog-close icon-button" aria-label="Close dashboard information" onClick={onClose}><X size={20}/></button>
  <span className="brand-icon"><Radio size={24}/></span><h2 id="help-title">For the Acmac fishing community.</h2>
  <p>This research dashboard serves small-scale fishermen and shore monitoring in Barangay Acmac, Iligan City, Lanao del Norte. All vessels, positions, weather readings and SOS events are fictional samples.</p>
  <p>Online and offline labels describe the captured demo snapshot. GPS positions are last reported fixes, never a guarantee of current location. All times use Philippine Standard Time (UTC+8).</p>
  <p>No live hardware or Supabase connection is active. The website cannot send SOS messages, dispatch rescue, or provide official weather forecasts. Vessel A models a sensor node; Vessel B models a GPS/LoRa node with unavailable local weather sensors.</p>
  <button className="button primary-button" onClick={onClose}>Back to the dashboard <ArrowRight size={16}/></button>
 </dialog>;
}
