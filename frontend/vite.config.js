import { defineConfig } from "vite";

export default defineConfig({
  base: "./", // rutas relativas: funciona en la raíz de Firebase Hosting y en cualquier subcarpeta
  build: { outDir: "../firebase/public", emptyOutDir: true, sourcemap: false },
  test: { environment: "node", include: ["tests/**/*.test.js"] },
});
