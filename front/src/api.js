const sessionKey = 'iot2.session'

export const session = {
  read: () => sessionStorage.getItem(sessionKey) || '',
  write(token) {
    if (token) sessionStorage.setItem(sessionKey, token)
    else sessionStorage.removeItem(sessionKey)
  },
}

export class ApiError extends Error {
  constructor(status) {
    super('API request failed')
    this.status = status
  }
}

export async function request(path, { method = 'GET', body, token } = {}) {
  let response
  try {
    response = await fetch(path, {
      method,
      cache: 'no-store',
      signal: AbortSignal.timeout(10000),
      headers: {
        Accept: 'application/json',
        ...(token && { Authorization: `Bearer ${token}` }),
        ...(body && { 'Content-Type': 'application/json' }),
      },
      body: body && JSON.stringify(body),
    })
  } catch {
    throw new ApiError(0)
  }
  if (!response.ok) throw new ApiError(response.status)
  if (response.status === 204) return null
  return response.json()
}

export function errorMessage(error, messages = {}) {
  return messages[error.status] ?? ({
    0: 'Не удалось подключиться к серверу. Проверьте, что сервис запущен.',
    401: 'Сессия завершилась. Войдите снова.',
    429: 'Слишком много запросов. Попробуйте чуть позже.',
  })[error.status] ?? 'Не удалось выполнить запрос. Попробуйте ещё раз.'
}
