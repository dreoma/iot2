<script setup>
defineProps({ name: { type: String, required: true }, size: { type: Number, default: 22 } })

const paths = {
  activity: 'M3 12h4l3-7 4 14 3-7h4',
  bell: 'M18 8a6 6 0 0 0-12 0c0 7-3 7-3 9h18c0-2-3-2-3-9M10 21h4',
  chevron: 'm9 6 6 6-6 6',
  close: 'm6 6 12 12M18 6 6 18',
  grid: 'M4 4h6v6H4zM14 4h6v6h-6zM4 14h6v6H4zM14 14h6v6h-6z',
  devices: 'M4 5h16v12H4zM8 21h8M12 17v4M8 10h2l1-2 2 5 1-2h2',
  chip: 'M7 7h10v10H7zM10 10h4v4h-4zM9 3v4m6-4v4M9 17v4m6-4v4M3 9h4m-4 6h4m10-6h4m-4 6h4',
  plus: 'M12 5v14M5 12h14',
  search: 'M10.5 17a6.5 6.5 0 1 1 0-13 6.5 6.5 0 0 1 0 13Zm5-1.5 4.5 4.5',
  refresh: 'M20 7v5h-5M4 17v-5h5M6 8a7 7 0 0 1 12-2l2 3M4 15l2 3a7 7 0 0 0 12-2',
  logout: 'M10 17l5-5-5-5m5 5H3m5-7V4h12v16H8v-1',
  clock: 'M12 22a10 10 0 1 0 0-20 10 10 0 0 0 0 20Zm0-15v5l3 2',
  menu: 'M4 6h16M4 12h16M4 18h16',
  back: 'm11 5-7 7 7 7M4 12h16',
  check: 'm5 12 4 4L19 6',
  info: 'M12 22a10 10 0 1 0 0-20 10 10 0 0 0 0 20Zm0-11v6m0-10v.01',
  mail: 'M3 5h18v14H3zM3 7l9 6 9-6',
  lock: 'M6 10h12v11H6zM9 10V7a3 3 0 1 1 6 0v3',
  eye: 'M2 12s4-6 10-6 10 6 10 6-4 6-10 6S2 12 2 12Zm10 2a2 2 0 1 0 0-4 2 2 0 0 0 0 4Z',
}
</script>

<template>
  <svg :width="size" :height="size" viewBox="0 0 24 24" fill="none" stroke="currentColor"
    stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
    <path :d="paths[name]" />
  </svg>
</template>
