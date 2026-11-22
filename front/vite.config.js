import { readFileSync } from 'node:fs'
import { Agent } from 'node:https'
import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

export default defineConfig(({ command }) => ({
  base: '/app/',
  plugins: [vue()],
  ...(command === 'serve' && {
    server: {
      proxy: {
        '^/(auth|users|devices)(/|$)': {
          target: 'https://127.0.0.1:8443',
          agent: new Agent({
            ca: readFileSync(new URL('../certs/ca.crt', import.meta.url)),
            servername: 'core-tls',
          }),
        },
      },
    },
  }),
}))
