"""Minimal local Telegram Bot API stub used by tests/compose.local.yml."""
import json
from email import policy
from email.parser import BytesParser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Lock
from time import sleep, time
from urllib.parse import parse_qs, urlsplit

LOCK = Lock()
UPDATES = []
REPLIES = {}
BOT = {"id": 456789, "is_bot": True, "first_name": "IoT Test", "username": "iot2_test_bot"}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def reply(self, result, status=200):
        content = json.dumps({"ok": status == 200, "result": result}).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def fields(self, params):
        content_type = self.headers.get("Content-Type", "")
        raw = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        if content_type.startswith("multipart/"):
            message = BytesParser(policy=policy.default).parsebytes(
                f"Content-Type: {content_type}\r\n\r\n".encode() + raw
            )
            return {
                part.get_param("name", header="content-disposition"): part.get_payload(decode=True).decode()
                for part in message.iter_parts()
            }
        if content_type.startswith("application/json"):
            return json.loads(raw)
        return {key: value[0] for key, value in parse_qs(params or raw.decode()).items()}

    def handle_request(self):
        url = urlsplit(self.path)
        data = self.fields(url.query)
        if url.path == "/start":
            user = {"id": int(data["user_id"]), "first_name": "Test", "is_bot": False}
            chat = {"id": data.get("chat_id", user["id"]), "type": data.get("chat_type", "private")}
            with LOCK:
                update_id = len(UPDATES) + 1
                UPDATES.append({"update_id": update_id, "message": {
                    "message_id": update_id, "date": int(time()), "from": user, "chat": chat,
                    "text": "/start " + data["payload"],
                    "entities": [{"type": "bot_command", "offset": 0, "length": 6}],
                }})
            self.reply(update_id)
        elif url.path == "/replies":
            with LOCK:
                replies = REPLIES.get(int(data["chat_id"]), []).copy()
            self.reply(replies)
        elif url.path == "/bot456789:test-token/getMe":
            self.reply(BOT)
        elif url.path == "/bot456789:test-token/getUpdates":
            offset = int(data.get("offset", 0))
            with LOCK:
                updates = [update for update in UPDATES if update["update_id"] >= offset]
            if not updates:
                sleep(0.25)
            self.reply(updates)
        elif url.path == "/bot456789:test-token/sendMessage":
            chat_id = int(data["chat_id"])
            with LOCK:
                REPLIES.setdefault(chat_id, []).append(data["text"])
            self.reply({
                "message_id": 1, "date": int(time()), "from": BOT,
                "chat": {"id": chat_id, "type": "private"}, "text": data["text"],
            })
        else:
            self.reply(None, 404)

    do_GET = do_POST = handle_request


if __name__ == "__main__":
    ThreadingHTTPServer(("0.0.0.0", 8085), Handler).serve_forever()
