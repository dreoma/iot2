const dateTime = new Intl.DateTimeFormat('ru-RU', { dateStyle: 'medium', timeStyle: 'short' })
const time = new Intl.DateTimeFormat('ru-RU', { hour: '2-digit', minute: '2-digit', second: '2-digit' })
const number = new Intl.NumberFormat('ru-RU', { maximumFractionDigits: 3 })

const labels = {
  temperature: 'Температура',
  humidity: 'Влажность',
  pressure: 'Давление',
  voltage: 'Напряжение',
  battery: 'Батарея',
  light: 'Освещённость',
}

export function fieldLabel(key) {
  return Object.hasOwn(labels, key) ? labels[key] : key
}

export function displayValue(value) {
  if (typeof value === 'number') return number.format(value)
  if (typeof value === 'boolean') return value ? 'Да' : 'Нет'
  if (value === null) return '—'
  return typeof value === 'string' ? value : JSON.stringify(value)
}

export function timestamp(value) {
  if (!value) return NaN
  // PostgreSQL может вернуть разделитель-пробел и краткое смещение +00.
  return Date.parse(value.replace(' ', 'T').replace(/([+-]\d{2})$/, '$1:00'))
}

export function displayDate(value, full = true) {
  const ms = timestamp(value)
  return Number.isFinite(ms) ? (full ? dateTime : time).format(ms) : '—'
}

export function metricEntries(data) {
  return Object.entries(data ?? {}).filter(([, value]) => typeof value !== 'object' || value === null)
}

export function chartFor(telemetry, key, width = 240, height = 56, padding = 3) {
  const values = (telemetry ?? []).slice(0, 40)
    .map((reading) => reading.data?.[key]).filter(Number.isFinite).reverse()
  if (values.length < 2) return null
  const min = Math.min(...values)
  const max = Math.max(...values)
  const points = values.map((value, index) => {
    const x = padding + index * (width - 2 * padding) / (values.length - 1)
    const y = max === min ? height / 2 : height - padding - (value - min) * (height - 2 * padding) / (max - min)
    return `${x.toFixed(2)},${y.toFixed(2)}`
  })
  return { line: points.join(' '), fill: `${padding},${height} ${points.join(' ')} ${width - padding},${height}` }
}
