<template>
  <svg class="signal-art" viewBox="0 0 500 270" preserveAspectRatio="xMidYMid slice" fill="none" aria-hidden="true">
    <defs>
      <pattern id="signal-grid" width="30" height="30" patternUnits="userSpaceOnUse">
        <path d="M0 0H30V30" stroke="#fff" stroke-opacity=".13" />
      </pattern>
      <linearGradient id="signal-shade" x1="60" y1="100" x2="500" y2="200" gradientUnits="userSpaceOnUse">
        <stop stop-color="#77465a" stop-opacity="0" />
        <stop offset="1" stop-color="#88566d" stop-opacity=".55" />
      </linearGradient>
      <linearGradient id="signal-line" x1="10" y1="140" x2="490" y2="140" gradientUnits="userSpaceOnUse">
        <stop stop-color="#e9a5ad" stop-opacity="0" />
        <stop offset=".35" stop-color="#e9a5ad" />
        <stop offset="1" stop-color="#ffd9d7" />
      </linearGradient>
    </defs>
    <path d="M0 0h500v270H0z" fill="url(#signal-shade)" />
    <path d="M65 0h435v270H65z" fill="url(#signal-grid)" />
    <circle cx="334" cy="138" r="84" stroke="#ffc2c9" stroke-opacity=".34" />
    <circle cx="334" cy="138" r="112" stroke="#ffc2c9" stroke-opacity=".18" stroke-dasharray="3 8" />
    <circle cx="334" cy="138" r="55" stroke="#ffc2c9" stroke-opacity=".16" />
    <path d="M0 146h75c31 0 32-30 55-30s28 53 51 53c31 0 35-88 62-88 29 0 32 111 60 111 23 0 25-54 46-54h150" stroke="url(#signal-line)" stroke-width="2.5" />
    <circle cx="243" cy="81" r="5" fill="#ffd9d7" />
    <circle cx="411" cy="138" r="5" fill="#ffd9d7" />
    <circle cx="411" cy="138" r="12" stroke="#ffd9d7" stroke-opacity=".52" />
    <path d="M391 39h12m-6-6v12M133 200h12m-6-6v12M462 214h12m-6-6v12" stroke="#ffd9d7" stroke-opacity=".6" />
  </svg>
</template>
