"""Local UI preview with disposable simulated cards; never connects to ESP32 hardware."""
import argparse
import json
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

PAGE = Path(__file__).resolve().parents[1] / "CardBackup" / "WebPage.h"
LOCK = threading.Lock()
STATE = dict(busy=False, readerReady=True, storageReady=True, hasBackup=True,
             phase="idle", message="已就绪，请将一张卡贴近读卡器。", detail="", diagnostics="",
             cardUid="", cardType="", sourceUid="AB0A1EBB", crc="1234ABCD",
             progress=0, total=0, armed=False, targetUid="", confirmation="",
             confirmationKind="", block0Eligible=True, replacementUid="AB0A1EBB",
             sourceBlock0="AB0A1EBB0408040003C234747F4CA890",
             remainingMs=0, ssid="ICCard-S3", token="local-preview-only", ip="192.168.4.1")
JOB = None
DEADLINE = 0
GENERATION = 0
SIMULATED_CARD_UID = "9EBA0902"
SIMULATED_KIND = "cuid"


def update():
    global JOB, DEADLINE, GENERATION, SIMULATED_CARD_UID
    now = time.monotonic()
    if JOB:
        action, start = JOB
        age = now - start
        if age < .7:
            STATE.update(phase="waiting", message="请将一张卡贴近读卡器，等待识别（最多 15 秒）。")
        elif age < 2:
            phase = "writing" if action in {"write", "write-block0", "write-cuid"} else "checking" if action.startswith("prepare-") else "reading"
            total = 1 if action in {"write-block0", "write-cuid"} or action.startswith("prepare-") else 47
            STATE.update(phase=phase, message="正在处理模拟卡片，请保持卡片不动。",
                         progress=min(total - 1, int((age-.7)*36)), total=total)
        else:
            JOB = None
            STATE.update(busy=False, phase="success", cardUid=SIMULATED_CARD_UID, cardType="MIFARE 1KB（模拟）",
                         message="操作完成。", progress=47, total=47)
            if action == "info":
                STATE.update(message="识别成功，请查看最近识别的卡片。", progress=0, total=0)
            elif action == "backup":
                STATE.update(hasBackup=True, sourceUid="AB0A1EBB", crc="1234ABCD", block0Eligible=True,
                             replacementUid="AB0A1EBB", sourceBlock0="AB0A1EBB0408040003C234747F4CA890",
                             message="备份成功，47 个用户数据块已保存，断电后仍保留。")
            elif action in {"restore", "prepare-block0", "prepare-auto", "prepare-cuid"}:
                if SIMULATED_CARD_UID == STATE["sourceUid"]:
                    STATE.update(phase="error", progress=0, total=0,
                                 message="目标与备份的卡号相同，无法区分原卡与副本，未准备写入。")
                elif action == "prepare-block0" and SIMULATED_KIND != "gen1a":
                    STATE.update(phase="error", progress=0, total=0,
                                 diagnostics="[GEN1A/HANDSHAKE] FAILED: simulated timeout",
                                 message="模拟卡片未通过 Gen1A 检查，没有写入。")
                else:
                    GENERATION += 1
                    DEADLINE = now + 30
                    kind = "data" if action == "restore" else "block0" if SIMULATED_KIND == "gen1a" and action != "prepare-cuid" else "cuid"
                    STATE.update(phase="confirm", armed=True, targetUid=SIMULATED_CARD_UID,
                                 confirmationKind=kind,
                                 confirmation=str(GENERATION), progress=0, total=0,
                                 message="普通认证和读取已通过，但尚未验证第 0 块可写。确认后才会实际尝试标准写入。" if kind == "cuid" else "模拟检查通过，尚未写入。请核对卡号并确认。",
                                 diagnostics="[STANDARD/PREPARED] UNVERIFIED: no WRITE command sent" if kind == "cuid" else "[READ/CHECK] OK: simulated read check passed")
            elif action == "write":
                STATE.update(message="恢复成功，47 / 47 个用户数据块已写入并校验。")
            elif action in {"write-block0", "write-cuid"}:
                if action == "write-cuid" and SIMULATED_KIND != "cuid":
                    STATE.update(phase="error", progress=0, total=1,
                                 diagnostics="[WRITE0/ACK] FAILED: simulated NAK",
                                 message="模拟标准写入被拒绝，不能据此认定厂家或卡型。")
                else:
                    SIMULATED_CARD_UID = STATE["replacementUid"]
                    STATE.update(cardUid=SIMULATED_CARD_UID, progress=1, total=1,
                                 message="模拟第 0 块写入成功，16 字节回读一致，重新识别的卡号已更新。")
            elif action == "verify":
                STATE.update(message="校验通过，47 / 47 个用户数据块完全一致。")
    if STATE["armed"]:
        STATE["remainingMs"] = max(0, int((DEADLINE-now)*1000))
        if now >= DEADLINE:
            STATE.update(armed=False, phase="expired", confirmation="", confirmationKind="", targetUid="",
                         message="确认已过期，没有写入。请重新检查目标卡。")
    else:
        STATE["remainingMs"] = 0


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def reply(self, status, value, content_type="application/json; charset=utf-8"):
        body = (json.dumps(value, ensure_ascii=False) if not isinstance(value, str) else value).encode()
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        route = urlsplit(self.path).path
        if route == "/":
            html = PAGE.read_text().split('R"ICWEB(', 1)[1].split(')ICWEB";', 1)[0]
            html = html.replace("</header>", f'</header><p class="notice" style="margin-top:18px">本地预览 · 模拟 {SIMULATED_KIND} 卡片，所有读写结果均为演示，未连接实物。刷新页面不会重置模拟卡号。</p>', 1)
            self.reply(200, html, "text/html; charset=utf-8")
        elif route == "/api/state":
            with LOCK:
                update()
                self.reply(200, STATE)
        elif route == "/api/backup":
            if self.headers.get("X-ICCard-Token") != STATE["token"]:
                self.reply(403, {"error": "请从操作页面下载。"})
                return
            blocks = {str(i): "00"*16 for i in range(64) if i % 4 != 3}
            blocks["0"] = STATE["sourceBlock0"]
            self.reply(200, dict(format="mifare-classic-1k-user-data-v1", uid=STATE["sourceUid"], sak=8,
                                 blocks=blocks))
        else:
            self.reply(404, {"error": "Not found"})

    def do_POST(self):
        global JOB
        if self.path != "/api/action":
            self.reply(404, {"error": "Not found"})
            return
        if self.headers.get("X-ICCard-Token") != STATE["token"]:
            self.reply(403, {"error": "页面已失效，请刷新后重试。"})
            return
        data = parse_qs(self.rfile.read(int(self.headers.get("Content-Length", 0))).decode())
        arg = lambda name: data.get(name, [""])[0]
        action = arg("action")
        with LOCK:
            update()
            if STATE["busy"]:
                self.reply(409, {"error": "设备正在操作，请等待完成。"})
                return
            if action in {"write", "write-block0", "write-cuid"} and (not STATE["armed"] or
                    STATE["confirmationKind"] != {"write": "data", "write-block0": "block0", "write-cuid": "cuid"}[action] or
                    arg("uid") != STATE["targetUid"] or arg("confirmation") != STATE["confirmation"]):
                self.reply(409, {"error": "写入确认已过期或目标已改变，请重新检查目标卡。"})
                return
            if action in {"restore", "verify", "prepare-block0", "prepare-auto", "prepare-cuid"} and not STATE["hasBackup"]:
                self.reply(409, {"error": "请先备份原卡。"})
                return
            if action == "erase-backup" and arg("confirmation") != "ERASE":
                self.reply(400, {"error": "请确认删除备份。"})
                return
            if action not in {"info", "backup", "restore", "write", "verify", "cancel", "erase-backup", "prepare-block0", "write-block0", "prepare-auto", "prepare-cuid", "write-cuid"}:
                self.reply(400, {"error": "不支持的操作。"})
                return
            STATE.update(armed=False, confirmationKind="", confirmation="", targetUid="", remainingMs=0, detail="", diagnostics="", progress=0, total=0)
            if action == "cancel":
                STATE.update(phase="idle", message="已取消待确认的写入。")
            elif action == "erase-backup":
                STATE.update(phase="success", message="已删除板上的备份。", hasBackup=False, block0Eligible=False,
                             sourceUid="", crc="", sourceBlock0="", replacementUid="")
            else:
                STATE.update(busy=True, phase="queued", message="操作已接收，正在开始…")
                JOB = (action, time.monotonic())
            self.reply(202, {"accepted": True})


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--card-type", choices=["gen1a", "cuid", "fixed"], default="cuid")
    args = parser.parse_args()
    SIMULATED_KIND = args.card_type
    print(f"Preview only (fake cards): http://127.0.0.1:{args.port}", flush=True)
    ThreadingHTTPServer(("127.0.0.1", args.port), Handler).serve_forever()
