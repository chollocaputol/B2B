import { metrics, type Metric, type Reading } from './data';
export default function TrendChart({ readings, metric }: { readings: Reading[]; metric: Metric }) {
 const values=readings.map(r=>r[metric]).filter((value):value is number=>value!==null&&Number.isFinite(value));
 const config=metrics[metric];
 if (!values.length) return <div className="chart-empty"><strong>No {config.label.toLowerCase()} readings available</strong><span>This vessel has no valid samples for this metric in the selected range.</span></div>;
 const min=Math.floor(Math.min(...values)-2),max=Math.ceil(Math.max(...values)+2);
 const x=(i:number)=>52+i/Math.max(1,readings.length-1)*826;
 const y=(value:number)=>210-(value-min)/(max-min)*172;
 const segments: string[][]=[];
 let segment: string[]=[];
 readings.forEach((r,i)=>{
  const value=r[metric];
  if(value===null||!Number.isFinite(value)){if(segment.length)segments.push(segment);segment=[];return;}
  segment.push(`${x(i).toFixed(2)},${y(value).toFixed(2)}`);
 });
 if(segment.length)segments.push(segment);
 const dateFormat:Intl.DateTimeFormatOptions={timeZone:'Asia/Manila',month:'short',day:'numeric',hour:'2-digit',minute:'2-digit',hour12:false};
 return <div className="chart-wrap"><svg className="trend-chart" viewBox="0 0 910 258" role="img" aria-labelledby="trend-title trend-description">
  <title id="trend-title">{config.label} history</title><desc id="trend-description">{values.length} valid sample readings. Minimum {Math.min(...values)} {config.unit}, maximum {Math.max(...values)} {config.unit}. Missing readings create gaps. Individual values are available in the table below.</desc>
  <defs><linearGradient id="chart-fill" x1="0" y1="0" x2="0" y2="1"><stop offset="0%" stopColor={config.color} stopOpacity="0.16"/><stop offset="100%" stopColor={config.color} stopOpacity="0.01"/></linearGradient></defs>
  {[0,1,2,3,4].map(i=>{const value=min+(max-min)*i/4;return <g key={i}><line x1="52" x2="878" y1={y(value)} y2={y(value)} stroke="#edf0f2" strokeDasharray="4 5"/><text className="y-axis-label" x="40" y={y(value)+4} textAnchor="end">{value.toFixed(metric==='signal'?0:1)}</text></g>})}
  {segments.map((points,i)=><g key={i}><path d={`M ${points.join(' L ')} L ${points.at(-1)!.split(',')[0]},210 L ${points[0].split(',')[0]},210 Z`} fill="url(#chart-fill)"/><polyline points={points.join(' ')} fill="none" stroke={config.color} strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round"/>{points.length===1&&<circle cx={points[0].split(',')[0]} cy={points[0].split(',')[1]} r="3" fill={config.color}/>}</g>)}
  {[0,1,2,3,4].map(i=>{const index=Math.round((readings.length-1)*i/4);return <text className={`x-axis-label tick-${i}`} key={i} x={x(index)} y="244" textAnchor={i===0?'start':i===4?'end':'middle'}>{new Date(readings[index].timestamp).toLocaleString('en-PH',dateFormat)}</text>})}
 </svg></div>;
}
