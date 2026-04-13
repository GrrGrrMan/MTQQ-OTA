#include "mqtt_trigger.h"
#include "cmd_registry.h"
#include "../Build/config.h"
#include "../Build/logger.h"
#include "../Build/Log/log_sink.h"
#include "../Build/OTA/ota_pull.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ─────────────────────────────────────────────────────────────────────────────
// mqtt_trigger.cpp — MQTT backbone.
//
// Subsystem commands (led:, servo:) are registered externally via
// cmd_register() — this file has zero knowledge of LED or Motor modules.
// Built-in commands handled here: gpio:, status, reset, ota.
// ─────────────────────────────────────────────────────────────────────────────

static WiFiClient   wifiClient;
static PubSubClient mqtt(wifiClient);

// ── Log sink — wired into logger.h at runtime ─────────────────────────────
void mqtt_log(const String &msg)
{
    if (!mqtt.connected()) return;
    String out = (msg.length() > CFG_MQTT_MAX_MSG)
                     ? msg.substring(0, CFG_MQTT_MAX_MSG)
                     : msg;
    mqtt.publish(CFG_MQTT_TOPIC_LOG, out.c_str());
}

void mqtt_publish(const char *topic, const String &payload)
{
    if (!mqtt.connected())
        return;
    // Truncate to buffer size just like mqtt_log does
    String out = (payload.length() > CFG_MQTT_MAX_MSG)
                     ? payload.substring(0, CFG_MQTT_MAX_MSG)
                     : payload;
    mqtt.publish(topic, out.c_str());
}

// ── Built-in command handlers ─────────────────────────────────────────────

static void handleGpioCmd(const String &msg)
{
    // gpio:read:<pin>
    // gpio:write:<pin>:<value>
    // gpio:pwm:<pin>:<duty>
    int first  = msg.indexOf(':');
    int second = msg.indexOf(':', first + 1);
    int third  = msg.indexOf(':', second + 1);

    String action = msg.substring(first + 1, second);
    int    pin    = msg.substring(second + 1,
                                  third == -1 ? (int)msg.length() : third).toInt();

    if (action == "read")
    {
        pinMode(pin, INPUT);
        LOG("[GPIO] pin " + String(pin) + " = " + String(digitalRead(pin)));
    }
    else if (action == "write" && third != -1)
    {
        int value = msg.substring(third + 1).toInt();
        pinMode(pin, OUTPUT);
        digitalWrite(pin, value ? HIGH : LOW);
        LOG("[GPIO] pin " + String(pin) + " set to " + String(value));
    }
    else if (action == "pwm" && third != -1)
    {
        int duty = msg.substring(third + 1).toInt();
        pinMode(pin, OUTPUT);
        analogWrite(pin, duty);
        LOG("[GPIO] pin " + String(pin) + " PWM duty " + String(duty));
    }
    else
    {
        LOG("[GPIO] Unknown — use gpio:read:<pin> | gpio:write:<pin>:<val> | gpio:pwm:<pin>:<duty>");
    }
}

static void handleStatusCmd(const String &)
{
    char buf[CFG_MQTT_MAX_MSG];
    snprintf(buf, sizeof(buf),
             "[STATUS] IP:%s  heap:%u  uptime:%lus",
             WiFi.localIP().toString().c_str(),
             ESP.getFreeHeap(),
             millis() / 1000UL);
    LOG(String(buf));
}

static void handleResetCmd(const String &)
{
    LOG("[CMD] Rebooting...");
    delay(500);
    ESP.restart();
}

static void handleOtaCmd(const String &)
{
    ota_pull_force();
    LOG("[OTA] Force triggered via MQTT");
}

// ── MQTT callbacks ────────────────────────────────────────────────────────
static void onMessage(char *topic, byte *payload, unsigned int length)
{
    if (length < 1) return;
    String msg = String((char *)payload).substring(0, length);

    if (strcmp(topic, CFG_MQTT_TOPIC_OTA) == 0)
    {
        if (msg == "1")
        {
            Serial.println("[MQTT] OTA trigger received");
            ota_pull_force();
        }
        return;
    }

    if (strcmp(topic, CFG_MQTT_TOPIC_CMD) == 0)
    {
        Serial.println("[MQTT] CMD: " + msg);
        if (!cmd_dispatch(msg))
            LOG("[CMD] Unknown: " + msg);
    }
}

static void reconnect()
{
    if (WiFi.status() != WL_CONNECTED || mqtt.connected()) return;

    Serial.print("[MQTT] Connecting...");
    if (mqtt.connect(CFG_MQTT_CLIENT))
    {
        LOG(" connected");
        mqtt.subscribe(CFG_MQTT_TOPIC_OTA);
        mqtt.subscribe(CFG_MQTT_TOPIC_CMD);
    }
    else
    {
        Serial.printf(" failed (rc=%d), will retry\n", mqtt.state());
    }
}

// ── Public API ────────────────────────────────────────────────────────────
void mqtt_trigger_init()
{
    // Wire LOG() → MQTT at runtime — no compile-time dependency needed.
    log_sink_register([](const String &msg) { mqtt_log(msg); });

    // Register built-in commands.
    cmd_register("gpio:",  handleGpioCmd);
    cmd_register("status", handleStatusCmd);
    cmd_register("reset",  handleResetCmd);
    cmd_register("ota",    handleOtaCmd);

    mqtt.setBufferSize(CFG_MQTT_BUFFER_SIZE);
    mqtt.setServer(CFG_MQTT_HOST, CFG_MQTT_PORT);
    mqtt.setCallback(onMessage);
    reconnect();

    LOG("[MQTT] Trigger ready");
}

void mqtt_trigger_handle()
{
    static bool wasConnected = false;

    if (!mqtt.connected())
    {
        if (wasConnected)
        {
            wasConnected = false;
            // Notify subsystems via the command registry so this file
            // stays decoupled — subsystems can register an "mqtt_disconnect"
            // handler if they need one, or handle it in their own handle().
            LOG("[MQTT] Connection lost");
        }
        static uint32_t lastRetry = 0;
        if ((millis() - lastRetry) > CFG_MQTT_RETRY_MS)
        {
            lastRetry = millis();
            reconnect();
        }
        return;
    }

    wasConnected = true;
    mqtt.loop();
}
