<script setup>
import { computed, onMounted, onUnmounted, ref } from 'vue'
import Icon from './Icon.vue'
import { errorMessage, request } from '../api.js'

const emit = defineEmits(['signed-in'])
const registrationKey = 'iot2.registration'
const steps = [
  { id: 'email', title: 'Укажите почту', label: 'Почта' },
  { id: 'telegram', title: 'Подтвердите Telegram', label: 'Telegram' },
  { id: 'password', title: 'Задайте пароль', label: 'Пароль' },
]
const registration = ref(restoreRegistration())
const step = ref(registration.value ? 'telegram' : 'login')
const email = ref(registration.value?.email ?? '')
const password = ref('')
const visible = ref(false)
const pending = ref(false)
const checking = ref(false)
const error = ref('')
const notice = ref('')
const registering = computed(() => step.value !== 'login')
const title = computed(() => steps.find((item) => item.id === step.value)?.title ?? 'Войдите в аккаунт')
let timer

function validLink(link) {
  return typeof link === 'string' && /^https:\/\/t\.me\/[a-zA-Z0-9_]+\?start=[0-9a-f]{64}$/.test(link)
}

function restoreRegistration() {
  try {
    const data = JSON.parse(sessionStorage.getItem(registrationKey))
    if (data && typeof data.email === 'string' && /^[0-9a-f]{64}$/.test(data.token) &&
        validLink(data.link) && data.expiresAt > Date.now()) return data
  } catch {
    // Повреждённую запись можно безопасно начать заново.
  }
  sessionStorage.removeItem(registrationKey)
  return null
}

function resetRegistration(nextStep) {
  registration.value = null
  sessionStorage.removeItem(registrationKey)
  step.value = nextStep
  password.value = ''
  visible.value = false
  error.value = ''
  notice.value = ''
}

function restart(message) {
  resetRegistration('email')
  error.value = message
}

async function checkTelegram(manual = false) {
  const current = registration.value
  if (step.value !== 'telegram' || !current || checking.value) return
  if (current.expiresAt <= Date.now()) {
    restart('Ссылка истекла. Начните регистрацию заново.')
    return
  }
  checking.value = true
  if (manual) error.value = ''
  try {
    const status = await request('/auth/register/status', { token: current.token })
    if (registration.value !== current) return
    if (status.telegram_confirmed) {
      error.value = ''
      step.value = 'password'
    }
  } catch (cause) {
    if (registration.value !== current) return
    if (cause.status === 404 || cause.status === 401) {
      restart('Ссылка истекла. Начните регистрацию заново.')
    } else if (manual) {
      error.value = errorMessage(cause)
    }
  } finally {
    checking.value = false
  }
}

function checkWhenVisible() {
  if (!document.hidden) checkTelegram()
}

onMounted(() => {
  checkWhenVisible()
  timer = window.setInterval(checkWhenVisible, 3000)
  document.addEventListener('visibilitychange', checkWhenVisible)
})
onUnmounted(() => {
  window.clearInterval(timer)
  document.removeEventListener('visibilitychange', checkWhenVisible)
})

async function submit() {
  error.value = ''
  notice.value = ''
  const mail = email.value.trim()
  if (step.value !== 'email') {
    const length = new TextEncoder().encode(password.value).length
    if (length > 256 || (step.value === 'password' && length < 15)) {
      error.value = 'Пароль должен занимать от 15 до 256 байт.'
      return
    }
  }
  const currentStep = step.value
  pending.value = true
  try {
    if (currentStep === 'email') {
      const data = await request('/auth/register/start', { method: 'POST', body: { mail } })
      if (!validLink(data.telegram_url)) throw new Error('invalid Telegram link')
      const attempt = {
        email: mail,
        link: data.telegram_url,
        token: data.registration_token,
        expiresAt: Date.now() + data.expires_in * 1000,
      }
      registration.value = attempt
      sessionStorage.setItem(registrationKey, JSON.stringify(attempt))
      step.value = 'telegram'
    } else if (currentStep === 'password') {
      const token = registration.value.token
      await request('/users', { method: 'POST', body: { password: password.value }, token })
      resetRegistration('login')
      notice.value = 'Аккаунт создан. Войдите с новым паролем.'
    } else {
      const result = await request('/auth/login', { method: 'POST', body: { mail, password: password.value } })
      password.value = ''
      emit('signed-in', result.token)
    }
  } catch (cause) {
    if (currentStep === 'password' && [401, 404, 409].includes(cause.status)) {
      restart('Регистрация истекла или почта либо Telegram уже привязаны. Войдите или начните заново.')
      return
    }
    error.value = errorMessage(cause, {
      400: 'Проверьте введённые данные.',
      401: 'Проверьте почту и пароль.',
      409: 'Эта почта уже зарегистрирована. Войдите в аккаунт.',
      ...(currentStep === 'email' && { 503: 'Регистрация через Telegram пока недоступна. Попробуйте позже.' }),
    })
  } finally {
    pending.value = false
  }
}
</script>

<template>
  <main class="auth-page">
    <section class="auth-form-card" aria-labelledby="auth-title">
      <div class="tabs auth-tabs" aria-label="Доступ к аккаунту">
        <button type="button" :aria-pressed="!registering" :disabled="pending" @click="registering && resetRegistration('login')">Вход</button>
        <button type="button" :aria-pressed="registering" :disabled="pending" @click="!registering && resetRegistration('email')">Регистрация</button>
      </div>
      <ol v-if="registering" class="registration-steps" aria-label="Шаги регистрации">
        <li v-for="(item, index) in steps" :key="item.id" :aria-current="item.id === step ? 'step' : undefined">
          <span>{{ index + 1 }}</span>{{ item.label }}
        </li>
      </ol>
      <h1 id="auth-title">{{ title }}</h1>

      <div v-if="step === 'telegram'" class="form-stack telegram-step">
        <p>Откройте бота, нажмите «Старт» и вернитесь сюда. Ссылка действует 10 минут.</p>
        <a class="button primary" :href="registration.link" target="_blank" rel="noopener noreferrer">Открыть Telegram</a>
        <p class="registration-wait" role="status">Ожидаем подтверждение от бота…</p>
        <p v-if="error" class="message error" role="alert">{{ error }}</p>
        <div class="registration-actions">
          <button type="button" class="text-button" @click="resetRegistration('email')">Изменить почту</button>
          <button type="button" class="text-button" :disabled="checking" @click="checkTelegram(true)">Проверить</button>
        </div>
      </div>

      <form v-else class="form-stack" @submit.prevent="submit">
        <label v-if="step !== 'password'" class="field">
          <span>Электронная почта</span>
          <input v-model="email" placeholder="name@example.com" type="email" maxlength="255" autocomplete="email" required />
        </label>
        <p v-if="step === 'password'" class="message success"><Icon name="check" :size="18" /> Telegram подтверждён</p>
        <div v-if="step !== 'email'" class="field">
          <label for="auth-password">Пароль</label>
          <div class="password-field">
            <input id="auth-password" v-model="password" :type="visible ? 'text' : 'password'"
              :autocomplete="step === 'password' ? 'new-password' : 'current-password'" maxlength="256" required />
            <button type="button" class="icon-button" :aria-label="visible ? 'Скрыть пароль' : 'Показать пароль'" @click="visible = !visible">
              <Icon name="eye" :size="19" />
            </button>
          </div>
          <small v-if="step === 'password'">От 15 до 256 байт.</small>
        </div>
        <p v-if="error" class="message error" role="alert">{{ error }}</p>
        <p v-if="notice" class="message success" role="status">{{ notice }}</p>
        <button class="button primary auth-submit" :disabled="pending">
          {{ pending ? 'Подождите…' : step === 'email' ? 'Продолжить' : step === 'password' ? 'Создать аккаунт' : 'Войти' }}
        </button>
      </form>
    </section>
  </main>
</template>
