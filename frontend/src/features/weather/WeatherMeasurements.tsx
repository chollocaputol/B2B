import { Activity, Clock3, Droplets, Gauge, Thermometer } from 'lucide-react';
import { dateTime, metrics, weatherLabel, type Reading } from '../../lib/data';

type Props = { latest: Reading | null | undefined; loading: boolean; countPending: boolean; study: boolean };
export default function WeatherMeasurements({ latest, loading, countPending, study }: Props) {
    return <section className="weather-measurements" aria-label="Latest sensor samples" aria-busy={loading}>
        <div className="stats-grid weather-measurement-grid">
            {(['temperature', 'humidity', 'pressure', 'pressureTrend'] as const).map(key => {
                const trend = key === 'pressureTrend';
                const Icon = key === 'temperature' ? Thermometer : key === 'humidity' ? Droplets : trend ? Activity : Gauge;
                const label = trend ? 'Pressure trend' : metrics[key].label;
                const unit = trend ? 'hPa' : metrics[key].unit;
                const value = latest?.[key];
                const valid = !countPending && typeof value === 'number' && Number.isFinite(value);
                return <article className={'stat-card ' + key} key={key}><div className="stat-top"><span>{label}</span><span className="stat-icon"><Icon size={18} /></span></div><div className="stat-value">{loading ? <span className="value-skeleton" aria-label="Loading sample" /> : valid ? (trend && value > 0 ? '+' : '') + value.toFixed(1) : '—'}<span>{unit}</span></div><div className="stat-foot">{loading ? 'Loading sample…' : valid ? (study ? 'Recorded sensor sample' : 'Fictional sensor sample') : 'Unavailable'}</div></article>;
            })}
        </div>
        <div className="weather-sample-context"><span><Clock3 size={14} />{loading ? 'Loading observation…' : countPending || !latest || !Number.isFinite(latest.timestamp) ? 'Observation time unavailable' : (study ? 'Captured: ' : 'Sample: ') + dateTime(latest.timestamp) + ' PHT'}</span><span className={'weather-state state-' + (countPending ? -1 : latest?.weatherState ?? -1)}>{loading ? 'Loading…' : countPending || !latest ? 'Unavailable' : weatherLabel(latest.weatherState)}</span></div>
        <p className="weather-measurement-note">Recorded sensor state only; not an official forecast or a guarantee of safe conditions.</p>
    </section>
}
