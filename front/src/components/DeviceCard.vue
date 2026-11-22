<script setup>
import { computed } from 'vue'
import Icon from './Icon.vue'
import { chartFor, displayDate, displayValue, fieldLabel, metricEntries } from '../telemetry.js'

const props = defineProps({ device: { type: Object, required: true } })
defineEmits(['open'])

const last = computed(() => props.device.telemetry?.[0])
const entries = computed(() => metricEntries(last.value?.data).slice(0, 3))
const chart = computed(() => {
  const metric = entries.value.find(([, value]) => Number.isFinite(value))
  return metric ? chartFor(props.device.telemetry, metric[0]) : null
})
</script>

<template>
  <article class="device-card">
    <div class="device-card-top">
      <span class="tile-icon leaf"><Icon name="chip" :size="23" /></span>
      <span class="status-pill" :class="{ 'has-data': last }"><span></span>{{ last ? 'Есть данные' : 'Ожидает данные' }}</span>
    </div>
    <h3><button class="device-link" type="button" @click="$emit('open', device.id)">{{ device.name }}</button></h3>
    <span class="device-id">ID {{ device.id }}</span>
    <dl v-if="entries.length" class="mini-metrics">
      <div v-for="[key, value] in entries" :key="key"><dt>{{ fieldLabel(key) }}</dt><dd>{{ displayValue(value) }}</dd></div>
    </dl>
    <div v-else class="card-no-metrics">{{ last ? 'Измерение сохранено' : 'Измерений пока нет' }}</div>
    <svg v-if="chart" class="mini-chart" viewBox="0 0 240 56" preserveAspectRatio="none"
      fill="none" aria-hidden="true">
      <polygon :points="chart.fill" fill="currentColor" opacity=".09" />
      <polyline :points="chart.line" stroke="currentColor" stroke-width="2" stroke-linejoin="round" vector-effect="non-scaling-stroke" />
    </svg>
    <div class="device-card-footer">
      <Icon name="clock" :size="16" />
      <span>{{ last ? displayDate(last.received_at) : 'Нет последнего измерения' }}</span>
      <Icon name="chevron" :size="17" />
    </div>
  </article>
</template>
