#pragma once
#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────
// mqtt_trigger.h — MQTT connection lifecycle and command dispatch.
//
// This module owns:
//   • WiFi+MQTT connection and reconnect logic
//   • Routing inbound CMD messages through cmd_registry
//   • Registering itself as the log sink so LOG() forwards over MQTT
//   • Built-in commands: gpio:, status, reset, ota
//
// Subsystem commands (led:, servo:) are registered by their own modules
// via cmd_register() — mqtt_trigger.cpp has no direct dependency on them.
// ─────────────────────────────────────────────────────────────────────────────

void mqtt_trigger_init();
void mqtt_trigger_handle();

// Forward a message to the MQTT log topic.
// Called internally by the log sink — prefer LOG() in application code.
void mqtt_log(const String &msg);

// Call ONLY from the main loop thread — PubSubClient is not thread-safe.
// The HRM module calls this from hrm_handle() which runs in loop().
void mqtt_publish(const char *topic, const String &payload);
