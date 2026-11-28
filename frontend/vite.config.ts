import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
export default defineConfig({
  plugins: [react()],
  build: {
    rollupOptions: {
      output: {
        // Stable dependency groups; retain tree-shaking and the default warning threshold.
        manualChunks: {
          react: ['react', 'react-dom'],
          map: ['leaflet'],
          supabase: ['@supabase/supabase-js'],
        },
      },
    },
  },
});
