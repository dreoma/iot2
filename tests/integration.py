"""Проверка контейнеров с tests/compose.local.yml: python3 tests/integration.py."""
from functools import partial
import json
import pathlib
import subprocess
import time
import os
from urllib.parse import parse_qs, urlsplit

ROOT = pathlib.Path(__file__).resolve().parents[1]


def request(method, path, body=None, adapter=False, expected=200, token=None):
    command = ["curl", "--noproxy", "*", "-sS", "--max-time", "10", "-X", method]
    if adapter:
        url = "http://127.0.0.1:18081" + path
    else:
        command += ["--cacert", str(ROOT / "certs/ca.crt"),
                    "--resolve", "core-tls:18443:127.0.0.1"]
        url = "https://core-tls:18443" + path
    if token:
        command += ["-H", f"Authorization: Bearer {token}"]
    if body is not None:
        command += ["-H", "Content-Type: application/json", "-d", json.dumps(body)]
    output = subprocess.check_output(command + ["-w", "\n%{http_code}", url], text=True)
    content, status = output.rsplit("\n", 1)
    assert int(status) == expected, (method, path, status, content)
    return json.loads(content) if content else None


def telegram_mock(path, body=None):
    command = ["curl", "--noproxy", "*", "-fsS", "--max-time", "5", "http://127.0.0.1:18085" + path]
    if body is not None:
        command += ["-H", "Content-Type: application/json", "-d", json.dumps(body)]
    return json.loads(subprocess.check_output(command, text=True))["result"]


def wait_for(check):
    for _ in range(40):
        result = check()
        if result:
            return result
        time.sleep(0.25)
    raise AssertionError("Telegram bot did not process the update")


def register(mail, telegram_id, password, verify_boundaries=False):
    attempt = request("POST", "/auth/register/start", {"mail": mail}, expected=201)
    assert attempt["expires_in"] == 600
    token = attempt["registration_token"]
    link = urlsplit(attempt["telegram_url"])
    assert (link.scheme, link.netloc, link.path) == ("https", "t.me", "/iot2_test_bot")
    payload, = parse_qs(link.query)["start"]
    assert len(payload) == 64 and payload != token
    if verify_boundaries:
        request("GET", "/auth/register/status", token=payload, expected=404)
        assert request("GET", "/auth/register/status", token=token) == {"telegram_confirmed": False}
        request("POST", "/users", {"password": password}, token=token, expected=409)
        # Даже совпадающий ID из неперсонального чата не должен привязываться.
        telegram_mock("/start", {"user_id": telegram_id + 2, "chat_type": "group", "payload": payload})
    telegram_mock("/start", {"user_id": telegram_id, "payload": payload})
    wait_for(lambda: request("GET", "/auth/register/status", token=token)["telegram_confirmed"])
    wait_for(lambda: telegram_mock(f"/replies?chat_id={telegram_id}"))
    if verify_boundaries:
        telegram_mock("/start", {"user_id": telegram_id + 2, "payload": payload})
        replies = wait_for(lambda: telegram_mock(f"/replies?chat_id={telegram_id + 2}"))
        assert "недействительна" in replies[0]
    user = request("POST", "/users", {"password": password}, token=token, expected=201)
    assert user["mail"] == mail and user["telegram_id"] == telegram_id
    request("GET", "/auth/register/status", token=token, expected=404)
    return user


stamp = time.time_ns() // 1000
mail = f"integration-{stamp}@example.com"
password = "Integration password for API"
user_id = device_id = other_user_id = None
api = other_api = None
rule_ids = []
try:
    request("GET", "/users", expected=401)
    request("GET", "/devices", expected=401)
    request("POST", "/users", {"mail": mail, "telegram_id": stamp, "password": password}, expected=400)
    user = register(mail, stamp, password, verify_boundaries=True)
    user_id = user["id"]
    assert "password" not in user and "password_hash" not in user
    credentials = {"mail": mail, "password": password}
    wrong_login = request("POST", "/auth/login", {**credentials, "password": "wrong"}, expected=401)
    unknown_login = request("POST", "/auth/login",
                            {**credentials, "mail": "absent@example.com"}, expected=401)
    assert wrong_login == unknown_login
    session = request("POST", "/auth/login", credentials)
    assert session["user_id"] == user_id and session["expires_in"] == 86400
    token = session["token"]
    api = partial(request, token=token)

    assert api("GET", f"/users/{user_id}")["telegram_id"] == stamp
    updated_mail = f"updated-{stamp}@example.com"
    api("PUT", f"/users/{user_id}", {"mail": updated_mail, "telegram_id": stamp + 9}, expected=400)
    assert api("PUT", f"/users/{user_id}", {"mail": updated_mail})["telegram_id"] == stamp
    device = api("POST", "/devices", {
        "name": "integration", "notifications_enabled": True
    }, expected=201)
    device_id = device["id"]
    api("PUT", f"/devices/{device_id}", {"name": "updated", "notifications_enabled": False})
    assert api("GET", f"/users/{user_id}")["users_devices"][0]["id"] == device_id
    data = {"temperature": 22.5, "humidity": 48}
    saved = request("POST", "/telemetry", {
        "device_id": device_id, "data": data
    }, adapter=True, expected=201)
    assert saved["data"] == data
    assert saved["user_id"] == user_id
    device = api("GET", f"/devices/{device_id}")
    assert device["telemetry"][0]["data"] == data
    assert device["telemetry"][0]["user_id"] == user_id
    assert device["notifications_enabled"] is False
    assert [d["id"] for d in api("GET", "/devices")] == [device_id]
    assert [u["id"] for u in api("GET", "/users")] == [user_id]
    request("POST", "/telemetry/encrypted", {"payload": "AAAA"}, expected=400)
    request("POST", "/telemetry", {}, adapter=True, expected=400)
    # Rule engine: порог, повторные значения, missing/string и флаг устройства.
    rule = {"device_id": device_id, "field": "temperature", "operator": "gt",
            "threshold": 30, "message": "Температура выше 30", "enabled": True}
    rule_ids.append(api("POST", "/rules", rule, expected=201)["id"])
    assert len(api("GET", f"/devices/{device_id}/rules")) == 1

    def telemetry(data, count):
        result = request("POST", "/telemetry", {"device_id": device_id, "data": data},
                         adapter=True, expected=201)
        assert result["rules_evaluated"] is True, result
        assert result["notifications_created"] == count, result
        assert result["device_id"] == device_id
        assert result["user_id"] == user_id

    telemetry({"temperature": 35}, 0)  # notifications_enabled=false
    api("PUT", f"/devices/{device_id}", {
        "name": "updated", "notifications_enabled": True
    })
    telemetry({"temperature": 20}, 0)
    telemetry({"temperature": 35}, 1)
    telemetry({"temperature": 36}, 0)
    telemetry({"temperature": 25}, 0)
    telemetry({"temperature": 40}, 1)
    telemetry({"humidity": 48}, 0)
    telemetry({"temperature": "hot"}, 0)
    notifications = api("GET", f"/users/{user_id}/notifications")
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
    api("PUT", f"/rules/{rule_ids[0]}", rule)
    telemetry({"temperature": 20}, 0)
    telemetry({"temperature": 35}, 0)
    api("POST", "/rules", {**rule, "operator": "invalid"}, expected=400)
    for operator, field in [("changed", "door"), ("delta", "pressure")]:
        rule_ids.append(api("POST", "/rules", {
            "device_id": device_id, "field": field, "operator": operator,
            "threshold": 5, "message": operator
        }, expected=201)["id"])
    telemetry({"door": False, "pressure": 10}, 0)
    telemetry({"door": True, "pressure": 16}, 2)
    telemetry({"door": True, "pressure": 17}, 0)
    # Снимок события не зависит от текущего имени устройства и настройки правила.
    api("PUT", f"/devices/{device_id}", {
        "name": "renamed-after-event", "notifications_enabled": True
    })
    history = api("GET", f"/users/{user_id}/notifications")
    historical = next(n for n in history if n["id"] == notifications[0]["id"])
    assert historical["context"] == context
    if os.getenv("CHECK_EMAIL") == "1":
        # Письма принимаются только локальным Mailpit, реальные адресаты не затрагиваются.
        recipient = f"updated-{stamp}@example.com"
        for _ in range(40):
            listing = json.loads(subprocess.check_output([
                "curl", "--noproxy", "*", "-fsS", "--max-time", "5",
                "http://127.0.0.1:18025/api/v1/messages"
            ], text=True))
            emails = [m for m in listing["messages"]
                      if any(to["Address"] == recipient for to in m["To"])]
            if len(emails) >= 4:
                break
            time.sleep(0.25)
        assert len(emails) == 4, emails
        full_messages = [json.loads(subprocess.check_output([
            "curl", "--noproxy", "*", "-fsS", "--max-time", "5",
            "http://127.0.0.1:18025/api/v1/message/" + m["ID"]
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
    other_mail = f"other-{stamp}@example.com"
    other = register(other_mail, stamp + 1, password)
    other_user_id = other["id"]
    other_session = request("POST", "/auth/login", {"mail": other_mail, "password": password})
    other_api = partial(request, token=other_session["token"])
    assert other_api("GET", "/devices") == []
    assert [u["id"] for u in other_api("GET", "/users")] == [other_user_id]
    for path in (f"/users/{user_id}", f"/users/{user_id}/notifications",
                 f"/devices/{device_id}", f"/devices/{device_id}/rules"):
        other_api("GET", path, expected=404)
    other_api("PUT", f"/devices/{device_id}", {"name": "another-owner"}, expected=404)
    other_api("DELETE", f"/devices/{device_id}", expected=404)
    other_api("POST", "/rules", rule, expected=404)
    other_api("DELETE", f"/rules/{rule_ids[0]}", expected=404)
    other_device = other_api("POST", "/devices", {"name": "other", "user_id": user_id}, expected=201)
    try:
        assert other_device["user_id"] == other_user_id
        other_api("PUT", f"/rules/{rule_ids[0]}", {**rule, "device_id": other_device["id"]}, expected=404)
    finally:
        other_api("DELETE", f'/devices/{other_device["id"]}', expected=204)
    assert api("PUT", f"/devices/{device_id}", {
        "name": "renamed-after-event", "user_id": other_user_id
    })["user_id"] == user_id
    assert next(n for n in api("GET", f"/users/{user_id}/notifications")
                if n["id"] == notifications[0]["id"])["context"] == context

    updated_password = "Different integration password"
    api("PUT", "/auth/password", {
        "current_password": "wrong", "new_password": updated_password
    }, expected=403)
    api("PUT", "/auth/password", {
        "current_password": password, "new_password": updated_password
    }, expected=204)
    api("GET", "/users", expected=401)
    request("POST", "/auth/login", {"mail": updated_mail, "password": password}, expected=401)
    credentials = {"mail": updated_mail, "password": updated_password}
    old_token = request("POST", "/auth/login", credentials)["token"]
    token = request("POST", "/auth/login", credentials)["token"]
    api = partial(request, token=token)
    request("GET", "/users", token=old_token, expected=401)
    api("POST", "/auth/logout", expected=204)
    api("GET", "/users", expected=401)
    api = partial(request, token=request("POST", "/auth/login", credentials)["token"])
    print("OK: Telegram registration, password login, session revocation, access control, telemetry and rules")
finally:
    if api is not None:
        for rule_id in rule_ids:
            api("DELETE", f"/rules/{rule_id}", expected=204)
        if device_id is not None:
            api("DELETE", f"/devices/{device_id}", expected=204)
            api("GET", f"/devices/{device_id}", expected=404)
        if user_id is not None:
            api("DELETE", f"/users/{user_id}", expected=204)
            api("GET", f"/users/{user_id}", expected=401)
    if other_api is not None:
        other_api("DELETE", f"/users/{other_user_id}", expected=204)
    if user_id is not None:
        # Удаляются только уведомления созданного этим тестом пользователя.
        subprocess.check_call([
            "docker", "compose", "exec", "-T", "rules-db", "psql",
            "-U", "rules_user", "-d", "iot_rules", "-c",
            f"DELETE FROM notifications WHERE user_id={int(user_id)}"
        ], cwd=ROOT, stdout=subprocess.DEVNULL)
