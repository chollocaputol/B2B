export type SnapshotPhase = 'loading' | 'ready' | 'error' | 'empty';
export function snapshotPhase(busy:boolean, failed:boolean, count:number):SnapshotPhase {
 return busy?'loading':failed?'error':count>0?'ready':'empty';
}
export function selectionFor(devices:readonly {id:string}[], preferred:string):string {
 return devices.some(d=>d.id===preferred)?preferred:devices[0]?.id??'';
}
export function isAccessFailure(status:number, code?:string):boolean {
 return status===401||status===403||code==='42501'||code==='PGRST301'||code==='PGRST302'||code==='PGRST303';
}
