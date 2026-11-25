// Only the approved hosted site/project and the retained local lab may open study access.
export function studyAccessMode(hostname: string, protocol: string, url: string | undefined): 'local' | 'hosted' | null {
  if (hostname === '127.0.0.1' && protocol === 'http:' && url?.trim() === 'http://127.0.0.1:54321') return 'local';
  if (hostname === 'bayanihan-b2b.netlify.app' && protocol === 'https:' && url?.trim() === 'https://twqumqiplqqfhixjvxeo.supabase.co') return 'hosted';
  return null;
}
