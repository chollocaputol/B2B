import { createClient, type SupabaseClient } from '@supabase/supabase-js';

let supabaseClient: SupabaseClient | undefined;

/** Return the shared browser client, initializing it only when first requested. */
export function getSupabaseClient(): SupabaseClient {
  if (supabaseClient) return supabaseClient;

  const url = import.meta.env.VITE_SUPABASE_URL?.trim();
  const publishableKey = import.meta.env.VITE_SUPABASE_PUBLISHABLE_KEY?.trim();
  const missing = [
    !url && 'VITE_SUPABASE_URL',
    !publishableKey && 'VITE_SUPABASE_PUBLISHABLE_KEY',
  ].filter(Boolean);

  if (missing.length > 0) {
    throw new Error(
      `Supabase is not configured. Set ${missing.join(' and ')} in frontend/.env.local (see frontend/.env.example).`,
    );
  }

  supabaseClient = createClient(url!, publishableKey!);
  return supabaseClient;
}
