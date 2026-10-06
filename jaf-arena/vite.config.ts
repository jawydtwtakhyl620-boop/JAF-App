import { defineConfig } from 'vite';

export default defineConfig({
  // Relative paths so the same build works on the web and inside the iOS/Android apps.
  base: './',
  build: { outDir: 'dist', chunkSizeWarningLimit: 1000 },
});
