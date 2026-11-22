<script setup>
import { displayDate, displayValue, fieldLabel } from '../telemetry.js'

defineProps({
  readings: { type: Array, required: true },
  showDevice: { type: Boolean, default: false },
})
defineEmits(['open'])
</script>

<template>
  <div class="table-wrap">
    <table class="reading-table">
      <thead><tr><th scope="col">Время измерения</th><th v-if="showDevice" scope="col">Устройство</th><th scope="col">Показания</th></tr></thead>
      <tbody>
        <tr v-for="reading in readings" :key="reading.id">
          <td><time :datetime="reading.received_at">{{ displayDate(reading.received_at) }}</time></td>
          <td v-if="showDevice"><button type="button" class="text-button" @click="$emit('open', reading.deviceId)">{{ reading.deviceName }}</button></td>
          <td>
            <div class="reading-values">
              <span v-for="(value, key) in reading.data" :key="key"><span>{{ fieldLabel(key) }}</span> <strong>{{ displayValue(value) }}</strong></span>
            </div>
          </td>
        </tr>
      </tbody>
    </table>
  </div>
</template>
