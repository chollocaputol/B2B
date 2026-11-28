import { useEffect, useRef, useState } from 'react';
import { Cpu, ChevronDown } from 'lucide-react';

// A disclosure with ordinary buttons, rather than an incomplete ARIA listbox.
export default function VesselSelector({ devices, selected, onSelect, placeholder }: { devices: { id: string; name: string }[]; selected: string; onSelect: (id: string) => void; placeholder: string }) {
    const [open, setOpen] = useState(false);
    const root = useRef<HTMLDivElement>(null), trigger = useRef<HTMLButtonElement>(null);
    const disabled = !devices.length;
    useEffect(() => { if (disabled) setOpen(false); }, [disabled]);
    useEffect(() => {
        if (!open) return;
        const outside = (e: PointerEvent) => { if (e.target instanceof Node && !root.current?.contains(e.target)) setOpen(false); };
        document.addEventListener('pointerdown', outside);
        return () => document.removeEventListener('pointerdown', outside);
    }, [open]);
    const name = devices.find(d => d.id === selected)?.name ?? placeholder;
    return <div className="vessel-picker" ref={root} onBlur={e => { if (!e.currentTarget.contains(e.relatedTarget as Node | null)) setOpen(false); }} onKeyDown={e => { if (e.key === 'Escape' && open) { e.preventDefault(); setOpen(false); trigger.current?.focus(); } }}>
        <button type="button" ref={trigger} className="vessel-picker-trigger" aria-label={`Select vessel: ${name}`} aria-expanded={open && !disabled} disabled={disabled} onClick={() => setOpen(v => !v)}><Cpu size={16} /><span>{name}</span><ChevronDown size={14} /></button>
        {open && !disabled && <div className="vessel-picker-options" aria-label="Vessel choices">{devices.map(d => <button type="button" key={d.id} aria-pressed={d.id === selected} onClick={() => { onSelect(d.id); setOpen(false); trigger.current?.focus(); }}>{d.name}{d.id === selected && <span aria-hidden="true">✓</span>}</button>)}</div>}
    </div>;
}
