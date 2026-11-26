export type Entry = 'landing' | 'operator' | 'legacy' | 'demo' | 'not-found';
export const sections = ['overview','vessels','weather','sos','records'] as const;
export type Section = typeof sections[number];
export const sectionLabels: Record<Section,string> = {overview:'Overview',vessels:'Vessels',weather:'Weather',sos:'SOS',records:'Telemetry history'};
function normalized(path:string) { return path.replace(/\/+$/, '') || '/'; }
export function sectionPath(section:Section,demo=false):string {
 const base=demo?'/demo':'/dashboard';
 return section==='overview'?base:`${base}/${section}`;
}
export function sectionAt(pathname:string,demo=false):Section|undefined {
 const path=normalized(pathname);
 return sections.find(section=>sectionPath(section,demo)===path);
}
export function sectionFromLocation(pathname:string,hash:string,demo=false):Section {
 const section=sectionAt(pathname,demo)??'overview';
 if(section!=='overview'||normalized(pathname)!==sectionPath('overview',demo)) return section;
 switch(hash){case '#devices':return 'vessels';case '#weather':return 'weather';case '#alerts':return 'sos';case '#readings':return 'records';default:return 'overview';}
}
export function operatorDestination(pathname:string,hash:string):string {
 return sectionPath(sectionFromLocation(pathname,hash));
}
export function entryRoute(pathname: string, search: string, hostname: string, development: boolean): Entry {
 const path = normalized(pathname);
 if (path === '/') return new URLSearchParams(search).get('study') === '1' ? 'legacy' : 'landing';
 if (path === '/login' || sectionAt(path)!==undefined) return 'operator';
 if (development && hostname === '127.0.0.1' && sectionAt(path,true)!==undefined) return 'demo';
 return 'not-found';
}
