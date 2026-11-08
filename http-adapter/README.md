# http-adapter

HTTP-прослойка для устройств. Она принимает телеметрию по `POST /telemetry` и
передаёт её в `iot-core` через HTTPS.

```bash
curl -X POST http://127.0.0.1:8081/telemetry \
  -H 'Content-Type: application/json' \
  -d '{"device_id":1,"data":{"temperature":22.5,"humidity":48}}'
```

Подробный общий запуск находится в [../README.md](../README.md).
