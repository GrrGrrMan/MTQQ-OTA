"""
ESP_remote.py — MQTT remote control + logger for ESP32 SpiderBot.

Usage:
    python ESP_remote.py

Built-in commands (not sent to board):
    log             — start streaming board logs to terminal
    exit            — stop log streaming
    help            — print full command reference
    help <section>  — print one section  (led / single / multi / pca / servo / gpio / sys)
    stop            — disconnect and quit
"""

import threading
from datetime import datetime

import paho.mqtt.client as mqtt

# ── Config — keep in sync with src/Build/config.h ────────────────────────────
BROKER = "broker.hivemq.com"
PORT = 1883
TOPIC_LOG = "beanspiderbot/log"
TOPIC_CMD = "beanspiderbot/cmd"
TOPIC_OTA = "beanspiderbot/ota/TG398Z"
# ─────────────────────────────────────────────────────────────────────────────

# ── Help sections ─────────────────────────────────────────────────────────────
_HELP: dict[str, str] = {
    #     "sys": """\
    # ┌─────────────────────────────────────────────────────────────┐
    # │  SYSTEM                                                     │
    # ├──────────────────────────┬──────────────────────────────────┤
    # │ LOCAL                    │                                  │
    # │  log                     │ stream board logs to terminal    │
    # │  exit                    │ stop log stream                  │
    # │  help [section]          │ this reference                   │
    # │  stop                    │ quit script                      │
    # ├──────────────────────────┼──────────────────────────────────┤
    # │ BOARD                    │                                  │
    # │  status                  │ IP / heap / uptime               │
    # │  reset                   │ reboot board                     │
    # │  ota                     │ force OTA firmware check         │
    # └──────────────────────────┴──────────────────────────────────┘
    # """,
    #     "led": """\
    # ┌─────────────────────────────────────────────────────────────┐
    # │  LED SUBSYSTEM OVERVIEW                                     │
    # │  All LED hardware shares the "led:" prefix.                 │
    # │  Routed by sub-module keyword:                              │
    # │                                                             │
    # │    led:single:<cmd>   onboard single LED (LEDC PWM)         │
    # │    led:multi:<cmd>    bare GPIO multi-LED (auto-detected)   │
    # │    led:pca:<cmd>      PCA9685 RGB LEDs                      │
    # │                                                             │
    # │  Type  help single | help multi | help pca  for details.   │
    # └─────────────────────────────────────────────────────────────┘
    # """,
    #     "single": """\
    # ┌─────────────────────────────────────────────────────────────┐
    # │  SINGLE LED  (led:single:*  —  onboard LEDC, channel 0)    │
    # ├──────────────────────────────────┬──────────────────────────┤
    # │  led:single:set:<duty>           │ brightness 0-255         │
    # │  led:single:fade:<target>:<ms>   │ fade to target over ms   │
    # │  led:single:loop:start:<gap>     │ |cmd1|cmd2|...           │
    # │  led:single:loop:stop            │ stop loop                │
    # │                                  │                          │
    # │  Loop inner commands (pipe-sep): │                          │
    # │    set:<duty>                    │ instant set              │
    # │    fade:<target>:<ms>            │ fade, then wait gap      │
    # │    stop                          │ fade to 0                │
    # └──────────────────────────────────┴──────────────────────────┘
    #   Shortcuts:  led:on  →  led:single:set:255
    #               led:off →  led:single:set:0
    # """,
    #     "multi": """\
    # ┌─────────────────────────────────────────────────────────────┐
    # │  MULTI GPIO LED  (led:multi:*  —  auto-detected pins)       │
    # ├──────────────────────────┬──────────────────────────────────┤
    # │ MODE SWITCHING           │                                  │
    # │  led:multi:mode:auto     │ breathe pattern, periodic scan   │
    # │  led:multi:mode:manual   │ MQTT-driven only, scan disabled  │
    # │  led:multi:mode:pca      │ GPIO LEDs off, PCA9685 takes over│
    # ├──────────────────────────┼──────────────────────────────────┤
    # │ DISCOVERY                │                                  │
    # │  led:multi:scan          │ re-scan candidate pins now       │
    # │  led:multi:enable:<pin>  │ force-add a pin to active list   │
    # │  led:multi:disable:<pin> │ remove pin from active list      │
    # │  led:multi:status        │ log mode + active pin list       │
    # ├──────────────────────────┼──────────────────────────────────┤
    # │ CONTROL  (manual mode)   │                                  │
    # │  led:multi:set:<pin>:<d> │ set one pin  0-255               │
    # │  led:multi:all:<duty>    │ set all active pins  0-255       │
    # └──────────────────────────┴──────────────────────────────────┘
    #   Shortcuts:  multi:auto   →  led:multi:mode:auto
    #               multi:manual →  led:multi:mode:manual
    #               multi:pca    →  led:multi:mode:pca
    # """,
    #     "pca": """\
    # ┌───────────────────────────────────────────────────────────────────┐
    # │  PCA9685 RGB LEDs  (led:pca:*  —  up to CFG_RGB_LED_COUNT)        │
    # ├──────────────────────────────────────┬────────────────────────────┤
    # │  led:pca:set:<id>:<r>:<g>:<b>        │ set one LED instantly      │
    # │  led:pca:fade:<id>:<r>:<g>:<b>:<ms>  │ software fade over ms      │
    # │  led:pca:all:<r>:<g>:<b>             │ all LEDs same colour       │
    # │  led:pca:off                         │ all off                    │
    # ├──────────────────────────────────────┼────────────────────────────┤
    # │  led:pca:pattern:blink:<id>:<r>:<g>:<b>:<on>:<off>               │
    # │  led:pca:pattern:pulse:<id>:<r>:<g>:<b>:<ms>                     │
    # │  led:pca:pattern:chase:<r>:<g>:<b>:<gap>                         │
    # │  led:pca:pattern:stop                │ stop pattern task          │
    # ├──────────────────────────────────────┴────────────────────────────┤
    # │  Shortcuts:  led:white →  led:pca:all:255:255:255                 │
    # │              led:black →  led:pca:off                             │
    # └───────────────────────────────────────────────────────────────────┘
    # """,
    #     "servo": """\
    # ┌─────────────────────────────────────────────────────────────┐
    # │  SERVO  (PCA9685, channels 0-15)                            │
    # ├──────────────────────────┬──────────────────────────────────┤
    # │  servo:<ch>:<angle>      │ channel 0-15, angle 0-180 deg    │
    # └──────────────────────────┴──────────────────────────────────┘
    # """,
    #     "gpio": """\
    # ┌─────────────────────────────────────────────────────────────┐
    # │  RAW GPIO                                                   │
    # ├────────────────────────────┬────────────────────────────────┤
    # │  gpio:read:<pin>           │ digital read                   │
    # │  gpio:write:<pin>:<val>    │ digital write (0 or 1)         │
    # │  gpio:pwm:<pin>:<duty>     │ PWM 0-255                      │
    # └────────────────────────────┴────────────────────────────────┘
    # """,
}

_ALL_SECTIONS = ["sys", "led", "single", "multi", "pca", "servo", "gpio"]


def _format_help(sections: list[str]) -> str:
    return "\n".join(_HELP[s] for s in sections if s in _HELP)


# ── Shortcut expansion ────────────────────────────────────────────────────────
_SHORTCUTS: list[tuple[str, str]] = [
    # # Single LED helpers
    # ("led:on", "led:single:set:255"),
    # ("led:off", "led:single:set:0"),
    # # PCA colour shortcuts
    # ("led:white", "led:pca:all:255:255:255"),
    # ("led:black", "led:pca:off"),
    # # Multi-IO mode shortcuts
    # ("multi:auto", "led:multi:mode:auto"),
    # ("multi:manual", "led:multi:mode:manual"),
    # ("multi:pca", "led:multi:mode:pca"),
]


def _expand(raw: str) -> str:
    """Return the expanded command string, or raw if no shortcut matches."""
    lower = raw.lower()
    for alias, expansion in _SHORTCUTS:
        if lower == alias or lower.startswith(alias + ":"):
            tail = raw[len(alias) :]
            return expansion + tail
    return raw


# ── Main class ────────────────────────────────────────────────────────────────
class Remote:
    def __init__(self):
        self.log_active = False
        self._local_cmds: dict[str, callable] = {}

        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message

        self._register_locals()

    # ── Local command registration ─────────────────────────────────────────
    def _register_locals(self):
        self._local_cmds["log"] = self._cmd_log_on
        self._local_cmds["exit"] = self._cmd_log_off
        self._local_cmds["help"] = self._cmd_help
        self._local_cmds["stop"] = self._cmd_stop

    def _cmd_log_on(self, _):
        self.log_active = True
        print("[monitor] Log streaming ON  — type 'exit' to stop")

    def _cmd_log_off(self, _):
        self.log_active = False
        print("[monitor] Log streaming OFF")

    def _cmd_help(self, raw: str):
        parts = raw.split()
        if len(parts) == 1:
            print(_format_help(_ALL_SECTIONS))
        else:
            section = parts[1].lower()
            if section in _HELP:
                print(_HELP[section])
            else:
                print(
                    f"[monitor] Unknown section '{section}'. "
                    f"Available: {', '.join(_ALL_SECTIONS)}"
                )

    def _cmd_stop(self, _):
        print("[monitor] Disconnecting...")
        self.client.disconnect()
        raise SystemExit

    # ── MQTT callbacks ─────────────────────────────────────────────────────
    def _on_connect(self, client, userdata, flags, rc, properties=None):
        if rc == 0:
            client.subscribe(TOPIC_LOG)
            print(f"[monitor] Connected to {BROKER}:{PORT}")
            # print(
            #     "[monitor] Type 'help' for reference, 'log' to stream board output.\n"
            #     "[monitor] Sections: sys / led / single / multi / pca / servo / gpio\n"
            # )
        else:
            print(f"[monitor] Connection failed (rc={rc})")

    def _on_disconnect(self, client, userdata, rc, properties=None):
        if rc != 0:
            print("[monitor] Unexpected disconnect — reconnecting...")

    def _on_message(self, client, userdata, msg):
        if not self.log_active:
            return
        ts = datetime.now().strftime("%H:%M:%S")
        payload = msg.payload.decode(errors="replace")
        print(f"[{ts}] {payload}")

    # ── Input loop ─────────────────────────────────────────────────────────
    def _input_loop(self):
        while True:
            try:
                raw = input().strip()
                if not raw:
                    continue

                keyword = raw.split(":")[0].split()[0].lower()

                if keyword in self._local_cmds:
                    self._local_cmds[keyword](raw)
                    continue

                if not self.client.is_connected():
                    print("[monitor] Not connected — command not sent")
                    continue

                expanded = _expand(raw)
                self.client.publish(TOPIC_CMD, expanded)

                if expanded != raw:
                    print(f"[sent] {expanded}  (expanded from '{raw}')")
                else:
                    print(f"[sent] {expanded}")

            except EOFError:
                break
            except SystemExit:
                raise
            except Exception as e:
                print(f"[monitor] Error: {e}")

    # ── Entry point ────────────────────────────────────────────────────────
    def run(self):
        print(f"[monitor] Connecting to {BROKER}:{PORT}...")
        self.client.connect_async(BROKER, PORT, keepalive=60)
        threading.Thread(target=self._input_loop, daemon=True).start()
        self.client.loop_forever(retry_first_connection=True)


if __name__ == "__main__":
    Remote().run()
