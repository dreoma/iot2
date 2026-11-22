<script setup>
import { computed, nextTick, onMounted, onUnmounted, ref } from 'vue'
import { errorMessage, request, session } from './api.js'
import { chartFor, displayDate, displayValue, fieldLabel, metricEntries, timestamp } from './telemetry.js'
import AuthPanel from './components/AuthPanel.vue'
import DeviceCard from './components/DeviceCard.vue'
import ReadingTable from './components/ReadingTable.vue'
import Icon from './components/Icon.vue'
import SignalArt from './components/SignalArt.vue'

const nav = [
  { id: 'overview', label: 'Обзор', icon: 'grid' },
  { id: 'devices', label: 'Устройства', icon: 'devices' },
  { id: 'telemetry', label: 'Телеметрия', icon: 'activity' },
]
const filters = [
  { id: 'all', label: 'Все устройства' },
  { id: 'with-data', label: 'С данными' },
  { id: 'empty', label: 'Без данных' },
]

const token = ref(session.read())
const profile = ref(null)
const devices = ref([])
const view = ref('overview')
const selectedId = ref(null)
const query = ref('')
const filter = ref('all')
const loading = ref(false)
const syncError = ref('')
const updatedAt = ref(null)
const accountError = ref('')
const logoutBusy = ref(false)
const toast = ref('')
const drawer = ref(false)
const createDialog = ref(null)
const deviceName = ref('')
const notifications = ref(true)
const createError = ref('')
const createBusy = ref(false)
let poll
let toastTimeout

const telemetryCount = computed(() => devices.value.filter((device) => device.telemetry?.length).length)
const lastReading = computed(() => devices.value
  .map((device) => device.telemetry?.[0]?.received_at)
  .filter((value) => Number.isFinite(timestamp(value)))
  .sort((a, b) => timestamp(b) - timestamp(a))[0])
const matchingDevices = computed(() => {
  const search = query.value.trim().toLocaleLowerCase('ru')
  return devices.value.filter((device) => device.name.toLocaleLowerCase('ru').includes(search) || String(device.id) === search)
})
const visibleDevices = computed(() => matchingDevices.value.filter((device) => {
  if (filter.value === 'all') return true
  return Boolean(device.telemetry?.length) === (filter.value === 'with-data')
}))
const readings = computed(() => matchingDevices.value
  .flatMap((device) => (device.telemetry ?? []).slice(0, 30).map((reading) => ({
    ...reading, deviceId: device.id, deviceName: device.name,
  })))
  .sort((a, b) => timestamp(b.received_at) - timestamp(a.received_at)).slice(0, 30))
const activeDevice = computed(() => devices.value.find((device) => device.id === selectedId.value))
const activeReading = computed(() => activeDevice.value?.telemetry?.[0])
const metrics = computed(() => metricEntries(activeReading.value?.data).map(([key, value]) => ({
  key, value, chart: Number.isFinite(value) ? chartFor(activeDevice.value.telemetry, key) : null,
})))
const title = computed(() => nav.find((item) => item.id === view.value)?.label)
const initial = computed(() => profile.value?.mail?.[0]?.toUpperCase() || '•')
const mark = import.meta.env.BASE_URL + 'mark.svg'

function navigate(page) {
  view.value = page
  selectedId.value = null
  filter.value = 'all'
  query.value = ''
  drawer.value = false
  window.scrollTo(0, 0)
}

function openDevice(id) {
  selectedId.value = id
  drawer.value = false
  window.scrollTo(0, 0)
}

function notify(message) {
  toast.value = message
  window.clearTimeout(toastTimeout)
  toastTimeout = window.setTimeout(() => { toast.value = '' }, 4000)
}

function clearSession() {
  createDialog.value?.close()
  token.value = ''
  session.write('')
  devices.value = []
  profile.value = null
  updatedAt.value = null
  syncError.value = ''
  accountError.value = ''
  loading.value = false
  navigate('overview')
}

function handlePrivateError(error, currentToken) {
  if (currentToken !== token.value) return true
  if (error.status !== 401) return false
  clearSession()
  notify('Сессия завершилась. Войдите снова.')
  return true
}

async function refresh() {
  if (!token.value || loading.value) return
  const currentToken = token.value
  loading.value = true
  try {
    const pending = [request('/devices', { token: currentToken })]
    if (!profile.value) pending.push(request('/users', { token: currentToken }))
    const [list, users] = await Promise.all(pending)
    if (currentToken !== token.value) return
    devices.value = list
    if (users) profile.value = users[0] ?? null
    updatedAt.value = new Date().toISOString()
    syncError.value = ''
  } catch (error) {
    if (!handlePrivateError(error, currentToken)) syncError.value = errorMessage(error)
  } finally {
    if (currentToken === token.value) loading.value = false
  }
}

function signedIn(newToken) {
  token.value = newToken
  session.write(newToken)
  refresh()
}

async function logout() {
  const currentToken = token.value
  logoutBusy.value = true
  accountError.value = ''
  try {
    await request('/auth/logout', { token: currentToken, method: 'POST' })
    if (currentToken === token.value) clearSession()
  } catch (error) {
    if (!handlePrivateError(error, currentToken)) accountError.value = errorMessage(error)
  } finally {
    logoutBusy.value = false
  }
}

async function showCreate() {
  deviceName.value = ''
  notifications.value = true
  createError.value = ''
  await nextTick()
  createDialog.value.showModal()
}

async function createDevice() {
  const name = deviceName.value.trim()
  if (!name) {
    createError.value = 'Введите название устройства.'
    return
  }
  if (loading.value) return
  const currentToken = token.value
  createBusy.value = true
  createError.value = ''
  try {
    const device = await request('/devices', {
      token: currentToken,
      method: 'POST',
      body: { name, notifications_enabled: notifications.value },
    })
    if (currentToken !== token.value) return
    devices.value = [...devices.value, device]
    createDialog.value.close()
    navigate('devices')
    notify('Устройство добавлено.')
  } catch (error) {
    if (!handlePrivateError(error, currentToken)) createError.value = errorMessage(error)
  } finally {
    createBusy.value = false
  }
}

function refreshWhenVisible() {
  if (!document.hidden && !createBusy.value) refresh()
}

onMounted(() => {
  refresh()
  poll = window.setInterval(refreshWhenVisible, 15000)
  document.addEventListener('visibilitychange', refreshWhenVisible)
})
onUnmounted(() => {
  window.clearInterval(poll)
  window.clearTimeout(toastTimeout)
  document.removeEventListener('visibilitychange', refreshWhenVisible)
})
</script>

<template>
  <header class="topbar">
    <div class="brand-row">
      <button v-if="token" class="icon-button menu-button" type="button" aria-label="Меню"
        :aria-expanded="drawer" aria-controls="navigation" @click="drawer = !drawer"><Icon name="menu" /></button>
      <a class="brand" href="/app/" @click.prevent="navigate('overview')">
        <img :src="mark" alt="" width="36" height="36" />
        <span>IoT <strong>Studio</strong></span>
      </a>
    </div>
    <div v-if="token" class="topbar-search">
      <label class="search">
        <Icon name="search" :size="20" />
        <input v-model="query" type="search" placeholder="Поиск устройств" aria-label="Поиск устройств" @input="selectedId = null" />
      </label>
    </div>
    <div v-if="token" class="topbar-account">
      <span class="avatar" :title="profile?.mail" aria-hidden="true">{{ initial }}</span>
      <button class="icon-button" type="button" aria-label="Выйти" title="Выйти" :disabled="logoutBusy" @click="logout"><Icon name="logout" /></button>
    </div>
  </header>

  <AuthPanel v-if="!token" @signed-in="signedIn" />

  <div v-else class="workspace">
    <button v-if="drawer" type="button" class="drawer-overlay" aria-label="Закрыть меню" @click="drawer = false"></button>
    <aside class="sidebar" :class="{ 'is-open': drawer }">
      <nav id="navigation" aria-label="Разделы">
        <button v-for="item in nav" :key="item.id" class="nav-item" type="button"
          :aria-current="view === item.id ? 'page' : undefined" @click="navigate(item.id)">
          <Icon :name="item.icon" :size="21" /><span>{{ item.label }}</span>
        </button>
      </nav>
      <div v-if="profile" class="sidebar-account">
        <span class="avatar" aria-hidden="true">{{ initial }}</span>
        <span><strong>Ваш аккаунт</strong><small :title="profile.mail">{{ profile.mail }}</small></span>
      </div>
    </aside>

    <main class="dashboard">
      <div class="content">
        <p v-if="accountError" class="message error" role="alert">{{ accountError }}</p>
        <div v-if="syncError" class="message error status-banner" role="alert">
          <span>{{ syncError }}</span>
          <button type="button" class="text-button" @click="refresh">Повторить</button>
        </div>

        <template v-if="selectedId !== null">
          <button type="button" class="text-button back-button" @click="selectedId = null"><Icon name="back" :size="19" /> {{ title }}</button>
          <template v-if="activeDevice">
            <div class="page-heading">
              <div><h1>{{ activeDevice.name }}</h1><p>ID {{ activeDevice.id }} · Уведомления {{ activeDevice.notifications_enabled ? 'включены' : 'выключены' }}</p></div>
              <button class="button secondary" type="button" :disabled="loading" @click="refresh"><Icon name="refresh" :size="19" /> Обновить</button>
            </div>
            <section class="detail-section" aria-labelledby="latest-heading">
              <div class="section-heading"><h2 id="latest-heading">Последнее измерение</h2><span v-if="activeReading">{{ displayDate(activeReading.received_at) }}</span></div>
              <div v-if="metrics.length" class="metric-grid">
                <article v-for="metric in metrics" :key="metric.key" class="metric-card">
                  <h3>{{ fieldLabel(metric.key) }}</h3>
                  <strong>{{ displayValue(metric.value) }}</strong>
                  <svg v-if="metric.chart" class="detail-chart" viewBox="0 0 240 56" preserveAspectRatio="none"
                    fill="none" role="img" :aria-label="`Динамика: ${fieldLabel(metric.key)}`">
                    <polygon :points="metric.chart.fill" fill="currentColor" opacity=".09" />
                    <polyline :points="metric.chart.line" stroke="currentColor" stroke-width="2" vector-effect="non-scaling-stroke" />
                  </svg>
                </article>
              </div>
              <div v-else class="empty-state compact"><Icon name="activity" :size="30" /><p>{{ activeReading ? 'Показания доступны в истории измерений.' : 'Ожидаем первое измерение от устройства.' }}</p></div>
            </section>
            <section v-if="activeReading" class="detail-section" aria-labelledby="history-heading">
              <div class="section-heading"><h2 id="history-heading">История измерений</h2><span>До 30 последних записей</span></div>
              <ReadingTable :readings="activeDevice.telemetry.slice(0, 30)" />
            </section>
          </template>
          <p v-else class="message" role="status">Устройство больше недоступно.</p>
        </template>

        <template v-else>
          <div class="page-heading">
            <div><h1>{{ view === 'devices' ? 'Мои устройства' : title }}</h1>
              <p>{{ view === 'telemetry' ? 'Последние измерения ваших устройств.' : 'Управляйте устройствами и следите за показаниями.' }}</p></div>
            <button type="button" class="button primary" @click="showCreate"><Icon name="plus" :size="20" /> Добавить устройство</button>
          </div>

          <template v-if="view === 'overview'">
            <section class="overview-hero" aria-labelledby="hero-heading">
              <SignalArt />
              <div class="overview-hero-copy">
                <span class="hero-label"><Icon name="activity" :size="18" /> Мониторинг</span>
                <h2 id="hero-heading">Телеметрия устройств</h2>
                <p>Просматривайте последние показания и историю измерений.</p>
                <button class="button hero-button" type="button" @click="navigate('telemetry')">Открыть телеметрию</button>
              </div>
            </section>
            <div class="stats" aria-label="Сводка">
              <div class="stat"><span class="tile-icon rose"><Icon name="devices" /></span><div><span>Всего устройств</span><strong>{{ updatedAt ? devices.length : '—' }}</strong></div></div>
              <div class="stat"><span class="tile-icon mint"><Icon name="activity" /></span><div><span>Устройств с данными</span><strong>{{ updatedAt ? telemetryCount : '—' }}</strong></div></div>
              <div class="stat"><span class="tile-icon lilac"><Icon name="clock" /></span><div><span>Последнее измерение</span><strong class="time-stat">{{ displayDate(lastReading) }}</strong></div></div>
            </div>
          </template>

          <section v-if="view !== 'telemetry'" class="device-section" aria-labelledby="device-heading">
            <div class="section-heading">
              <h2 id="device-heading">{{ view === 'overview' ? 'Мои устройства' : 'Список устройств' }}</h2>
              <button class="text-button" type="button" :disabled="loading" @click="refresh"><Icon name="refresh" :size="18" /> {{ loading ? 'Обновляем…' : 'Обновить' }}</button>
            </div>
            <div class="tabs filter-tabs" aria-label="Фильтр устройств">
              <button v-for="option in filters" :key="option.id" type="button" :aria-pressed="filter === option.id"
                @click="filter = option.id">{{ option.label }}</button>
            </div>
            <div v-if="visibleDevices.length" class="device-grid">
              <DeviceCard v-for="device in visibleDevices" :key="device.id" :device="device" @open="openDevice" />
            </div>
            <div v-else-if="loading && !updatedAt" class="empty-state compact" role="status">Загружаем устройства…</div>
            <div v-else-if="!syncError || updatedAt" class="empty-state">
              <span class="tile-icon rose"><Icon :name="devices.length ? 'search' : 'chip'" :size="25" /></span>
              <h3>{{ devices.length ? 'Устройства не найдены' : 'Добавьте первое устройство' }}</h3>
              <p>{{ devices.length ? 'Измените запрос или выбранный фильтр.' : 'После подключения датчика здесь появятся его измерения.' }}</p>
              <button v-if="!devices.length" class="button secondary" type="button" @click="showCreate"><Icon name="plus" :size="19" /> Добавить устройство</button>
            </div>
          </section>

          <section v-else class="detail-section feed-section" aria-labelledby="feed-heading">
            <div class="section-heading"><h2 id="feed-heading">История измерений</h2>
              <button class="text-button" type="button" :disabled="loading" @click="refresh"><Icon name="refresh" :size="18" /> {{ loading ? 'Обновляем…' : 'Обновить' }}</button></div>
            <ReadingTable v-if="readings.length" :readings="readings" show-device @open="openDevice" />
            <div v-else-if="loading && !updatedAt" class="empty-state compact" role="status">Загружаем измерения…</div>
            <div v-else-if="!syncError || updatedAt" class="empty-state">
              <span class="tile-icon rose"><Icon name="activity" :size="25" /></span><h3>Измерений пока нет</h3>
              <p>{{ query ? 'Нет измерений по этому запросу.' : 'История появится после первого пакета телеметрии.' }}</p>
            </div>
          </section>
        </template>

        <div v-if="updatedAt" class="updated-time"><Icon name="refresh" :size="15" /> Обновлено в {{ displayDate(updatedAt, false) }} · Каждые 15 секунд</div>
      </div>
    </main>

    <dialog ref="createDialog" class="dialog" aria-labelledby="create-title" @cancel="createBusy && $event.preventDefault()">
      <form class="form-stack" @submit.prevent="createDevice">
        <div class="dialog-heading"><h2 id="create-title">Новое устройство</h2>
          <button type="button" class="icon-button" aria-label="Закрыть" :disabled="createBusy" @click="createDialog.close()"><Icon name="close" /></button></div>
        <label class="field"><span>Название</span><input v-model="deviceName" maxlength="200" placeholder="Например, датчик в офисе" required autofocus /></label>
        <label class="checkbox"><input v-model="notifications" type="checkbox" /><span>Включить уведомления</span></label>
        <p v-if="createError" class="message error" role="alert">{{ createError }}</p>
        <div class="dialog-actions">
          <button type="button" class="button secondary" :disabled="createBusy" @click="createDialog.close()">Отмена</button>
          <button class="button primary" :disabled="createBusy || loading">{{ createBusy ? 'Добавляем…' : 'Создать' }}</button>
        </div>
      </form>
    </dialog>
  </div>

  <div v-if="toast" class="toast" role="status"><Icon name="info" :size="19" />{{ toast }}</div>
</template>
