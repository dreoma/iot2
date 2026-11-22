<script setup>
import { computed, ref } from 'vue'
import Icon from './Icon.vue'
import SignalArt from './SignalArt.vue'
import { errorMessage, request } from '../api.js'

const emit = defineEmits(['signed-in'])
const register = ref(false)
const email = ref('')
const password = ref('')
const telegram = ref('')
const visible = ref(false)
const pending = ref(false)
const error = ref('')
const notice = ref('')
const title = computed(() => register.value ? 'Создайте аккаунт' : 'Войдите в аккаунт')

function switchMode() {
  register.value = !register.value
  password.value = ''
  visible.value = false
  error.value = ''
  notice.value = ''
}

async function submit() {
  error.value = ''
  notice.value = ''
  const credentials = { mail: email.value.trim(), password: password.value }
  const length = new TextEncoder().encode(password.value).length
  const chatId = Number(telegram.value)
  if (length > 256 || (register.value && length < 15)) {
    error.value = 'Пароль должен занимать от 15 до 256 байт.'
    return
  }
  if (register.value && (!/^-?\d+$/.test(telegram.value) || !Number.isSafeInteger(chatId) || !chatId)) {
    error.value = 'Введите Telegram ID числом.'
    return
  }
  pending.value = true
  try {
    if (register.value) {
      await request('/users', { method: 'POST', body: { ...credentials, telegram_id: chatId } })
      register.value = false
      password.value = ''
      visible.value = false
      notice.value = 'Аккаунт создан. Теперь войдите с вашим паролем.'
      return
    }
    const result = await request('/auth/login', { method: 'POST', body: credentials })
    password.value = ''
    emit('signed-in', result.token)
  } catch (cause) {
    error.value = errorMessage(cause, {
      400: 'Проверьте введённые данные.',
      401: 'Проверьте почту и пароль.',
      409: 'Такая почта или Telegram ID уже используются.',
    })
  } finally {
    pending.value = false
  }
}
</script>

<template>
  <main class="auth-page">
    <section class="auth-hero" aria-labelledby="welcome-title">
      <SignalArt />
      <div class="auth-hero-copy">
        <Icon name="activity" :size="36" />
        <h1 id="welcome-title">Устройства и телеметрия в одном месте</h1>
        <p>Подключайте датчики и следите за их показаниями.</p>
      </div>
    </section>
    <section class="auth-form-card" aria-labelledby="auth-title">
      <div class="tabs auth-tabs" aria-label="Доступ к аккаунту">
        <button type="button" :aria-pressed="!register" :disabled="pending" @click="register && switchMode()">Вход</button>
        <button type="button" :aria-pressed="register" :disabled="pending" @click="!register && switchMode()">Регистрация</button>
      </div>
      <h2 id="auth-title">{{ title }}</h2>
      <form class="form-stack" @submit.prevent="submit">
        <label class="field">
          <span>Электронная почта</span>
          <input v-model="email" placeholder="name@example.com" type="email" maxlength="255" autocomplete="email" required />
        </label>
        <label v-if="register" class="field">
          <span>Telegram ID</span>
          <input v-model.trim="telegram" placeholder="123456789" type="text" inputmode="numeric" maxlength="17" required />
          <small>Числовой ID чата для уведомлений.</small>
        </label>
        <div class="field">
          <label for="auth-password">Пароль</label>
          <div class="password-field">
            <input id="auth-password" v-model="password" :type="visible ? 'text' : 'password'"
              :autocomplete="register ? 'new-password' : 'current-password'" maxlength="256" required />
            <button type="button" class="icon-button" :aria-label="visible ? 'Скрыть пароль' : 'Показать пароль'" @click="visible = !visible">
              <Icon name="eye" :size="19" />
            </button>
          </div>
          <small v-if="register">Минимум 15 байт.</small>
        </div>
        <p v-if="error" class="message error" role="alert">{{ error }}</p>
        <p v-if="notice" class="message success" role="status">{{ notice }}</p>
        <button class="button primary auth-submit" :disabled="pending">{{ pending ? 'Подождите…' : register ? 'Создать аккаунт' : 'Войти' }}</button>
      </form>
    </section>
  </main>
</template>
