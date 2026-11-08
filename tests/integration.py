"""Проверка запущенных контейнеров: python3 tests/integration.py."""
import json
import pathlib
import subprocess
import time
import os

ROOT = pathlib.Path(__file__).resolve().parents[1]


def request(method, path, body=None, adapter=False, expected=200):
    command = ["curl", "--noproxy", "*", "-sS", "--max-time", "10", "-X", method]
    if adapter:
        url = "http://127.0.0.1:8081" + path
    else:
        command += ["--cacert", str(ROOT / "certs/ca.crt"),
                    "--resolve", "core-tls:8443:127.0.0.1"]
        url = "https://core-tls:8443" + path
    if body is not None:
        command += ["-H", "Content-Type: application/json", "-d", json.dumps(body)]
    output = subprocess.check_output(command + ["-w", "\n%{http_code}", url], text=True)
    content, status = output.rsplit("\n", 1)
    assert int(status) == expected, (method, path, status, content)
    return json.loads(content) if content else None


stamp = time.time_ns()
user_id = device_id = None
other_user_id = None
rule_ids = []
try:
    user = request("POST", "/users", {
        "mail": f"integration-{stamp}@example.com", "telegram_id": stamp
    }, expected=201)
    user_id = user["id"]
    assert request("GET", f"/users/{user_id}")["telegram_id"] == stamp
    request("PUT", f"/users/{user_id}", {
        "mail": f"updated-{stamp}@example.com", "telegram_id": stamp
    })
    device = request("POST", "/devices", {
        "name": "integration", "user_id": user_id, "notifications_enabled": True
    }, expected=201)
    device_id = device["id"]
    request("PUT", f"/devices/{device_id}", {
        "name": "updated", "user_id": user_id, "notifications_enabled": False
    })
    assert request("GET", f"/users/{user_id}")["users_devices"][0]["id"] == device_id
    data = {"temperature": 22.5, "humidity": 48}
    saved = request("POST", "/telemetry", {
        "device_id": device_id, "data": data
    }, adapter=True, expected=201)
    assert saved["data"] == data
    assert saved["user_id"] == user_id
    device = request("GET", f"/devices/{device_id}")
    assert device["telemetry"][0]["data"] == data
    assert device["telemetry"][0]["user_id"] == user_id
    assert device["notifications_enabled"] is False
    assert isinstance(request("GET", "/devices"), list)
    assert isinstance(request("GET", "/users"), list)
    request("POST", "/telemetry/encrypted", {"payload": "AAAA"}, expected=400)
    request("POST", "/telemetry", {}, adapter=True, expected=400)
    # Rule engine: порог, повторные значения, missing/string и флаг устройства.
    rule = {"device_id": device_id, "field": "temperature", "operator": "gt",
            "threshold": 30, "message": "Температура выше 30", "enabled": True}
    rule_ids.append(request("POST", "/rules", rule, expected=201)["id"])
    assert len(request("GET", f"/devices/{device_id}/rules")) == 1

    def telemetry(data, count, owner_id=None):
        result = request("POST", "/telemetry", {"device_id": device_id, "data": data},
                         adapter=True, expected=201)
        assert result["rules_evaluated"] is True, result
        assert result["notifications_created"] == count, result
        assert result["device_id"] == device_id
        assert result["user_id"] == (user_id if owner_id is None else owner_id)

    telemetry({"temperature": 35}, 0)  # notifications_enabled=false
    request("PUT", f"/devices/{device_id}", {
        "name": "updated", "user_id": user_id, "notifications_enabled": True
    })
    telemetry({"temperature": 20}, 0)
    telemetry({"temperature": 35}, 1)
    telemetry({"temperature": 36}, 0)
    telemetry({"temperature": 25}, 0)
    telemetry({"temperature": 40}, 1)
    telemetry({"humidity": 48}, 0)
    telemetry({"temperature": "hot"}, 0)
    notifications = request("GET", f"/users/{user_id}/notifications")
    assert len(notifications) == 2
    assert notifications[0]["telegram_id"] == stamp
    assert notifications[0]["message"] == rule["message"]
    assert notifications[0]["previous_value"] == 25
    assert notifications[0]["current_value"] == 40
    context = notifications[0]["context"]
    assert context["device"] == {"id": device_id, "name": "updated"}
    assert context["user"]["id"] == user_id
    assert context["user"]["mail"] == f"updated-{stamp}@example.com"
    assert context["user"]["telegram_id"] == stamp
    assert context["rule"]["id"] == rule_ids[0]
    assert context["rule"]["operator"] == "gt" and context["rule"]["threshold"] == 30
    assert context["telemetry"]["id"] == notifications[0]["telemetry_id"]
    assert context["telemetry"]["data"] == {"temperature": 40}
    assert context["telemetry"]["previous_data"] == {"temperature": 25}
    assert context["occurred_at"]
    rule["enabled"] = False
    request("PUT", f"/rules/{rule_ids[0]}", rule)
    telemetry({"temperature": 20}, 0)
    telemetry({"temperature": 35}, 0)
    request("POST", "/rules", {**rule, "operator": "invalid"}, expected=400)
    for operator, field in [("changed", "door"), ("delta", "pressure")]:
        rule_ids.append(request("POST", "/rules", {
            "device_id": device_id, "field": field, "operator": operator,
            "threshold": 5, "message": operator
        }, expected=201)["id"])
    telemetry({"door": False, "pressure": 10}, 0)
    telemetry({"door": True, "pressure": 16}, 2)
    telemetry({"door": True, "pressure": 17}, 0)
    # Снимок события не зависит от текущего имени устройства и настройки правила.
    request("PUT", f"/devices/{device_id}", {
        "name": "renamed-after-event", "user_id": user_id, "notifications_enabled": True
    })
    history = request("GET", f"/users/{user_id}/notifications")
    historical = next(n for n in history if n["id"] == notifications[0]["id"])
    assert historical["context"] == context
    if os.getenv("CHECK_EMAIL") == "1":
        # Письма принимаются только локальным Mailpit, реальные адресаты не затрагиваются.
        recipient = f"updated-{stamp}@example.com"
        for _ in range(40):
            listing = json.loads(subprocess.check_output([
                "curl", "--noproxy", "*", "-fsS", "--max-time", "5",
                "http://127.0.0.1:8025/api/v1/messages"
            ], text=True))
            emails = [m for m in listing["messages"]
                      if any(to["Address"] == recipient for to in m["To"])]
            if len(emails) >= 4:
                break
            time.sleep(0.25)
        assert len(emails) == 4, emails
        full_messages = [json.loads(subprocess.check_output([
            "curl", "--noproxy", "*", "-fsS", "--max-time", "5",
            "http://127.0.0.1:8025/api/v1/message/" + m["ID"]
        ], text=True)) for m in emails]
        assert any(rule["message"] in m["Text"] and rule["message"] in m["HTML"]
                   for m in full_messages)
        assert all("updated" in m["Text"] and "→" in m["Text"] for m in full_messages)
        assert all(f"ID {device_id}" in m["Text"] and f"#{user_id}" in m["Text"]
                   and "Измерение: #" in m["Text"] and "Правило: #" in m["Text"]
                   for m in full_messages)
        assert all("ID измерения" in m["HTML"] and "ID владельца" in m["HTML"]
                   and "Условие" in m["HTML"] for m in full_messages)
        print("OK: 4 rule notifications delivered through SMTP; text + HTML verified in Mailpit")
    other = request("POST", "/users", {
        "mail": f"other-{stamp}@example.com", "telegram_id": stamp + 1
    }, expected=201)
    other_user_id = other["id"]
    request("PUT", f"/devices/{device_id}", {
        "name": "another-owner", "user_id": other_user_id, "notifications_enabled": True
    })
    # Старые правила не должны отправляться новому владельцу.
    telemetry({"door": False, "pressure": 99}, 0, other_user_id)
    new_rule = request("POST", "/rules", {
        "device_id": device_id, "field": "temperature", "operator": "gt",
        "threshold": 30, "message": "Для нового владельца"
    }, expected=201)
    rule_ids.append(new_rule["id"])
    telemetry({"temperature": 35}, 1, other_user_id)
    new_history = request("GET", f"/users/{other_user_id}/notifications")
    assert len(new_history) == 1
    assert new_history[0]["context"]["user"]["id"] == other_user_id
    assert new_history[0]["context"]["device"]["name"] == "another-owner"
    assert next(n for n in request("GET", f"/users/{user_id}/notifications")
                if n["id"] == notifications[0]["id"])["context"] == context
    print("OK: CRUD, encryption, rule notifications, event snapshots and ownership changes")
finally:
    for rule_id in rule_ids:
        request("DELETE", f"/rules/{rule_id}", expected=204)
    if user_id is not None:
        # Удаляются только уведомления созданного этим тестом пользователя.
        subprocess.check_call([
            "docker", "compose", "exec", "-T", "rules-db", "psql",
            "-U", "rules_user", "-d", "iot_rules", "-c",
            f"DELETE FROM notifications WHERE user_id={int(user_id)}"
        ], cwd=ROOT, stdout=subprocess.DEVNULL)
    if device_id is not None:
        request("DELETE", f"/devices/{device_id}", expected=204)
        request("GET", f"/devices/{device_id}", expected=404)
    if user_id is not None:
        request("DELETE", f"/users/{user_id}", expected=204)
        request("GET", f"/users/{user_id}", expected=404)
    if other_user_id is not None:
        subprocess.check_call([
            "docker", "compose", "exec", "-T", "rules-db", "psql",
            "-U", "rules_user", "-d", "iot_rules", "-c",
            f"DELETE FROM notifications WHERE user_id={int(other_user_id)}"
        ], cwd=ROOT, stdout=subprocess.DEVNULL)
        request("DELETE", f"/users/{other_user_id}", expected=204)
