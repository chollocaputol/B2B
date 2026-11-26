export type Entry = 'landing' | 'operator' | 'legacy' | 'demo' | 'not-found';
export function entryRoute(pathname: string, search: string, hostname: string, development: boolean): Entry {
 const path = pathname.replace(/\/+$/, '') || '/';
 if (path === '/') return new URLSearchParams(search).get('study') === '1' ? 'legacy' : 'landing';
 if (path === '/login' || path === '/dashboard') return 'operator';
 if (path === '/demo' && development && hostname === '127.0.0.1') return 'demo';
 return 'not-found';
}
