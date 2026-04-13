#include <Arduino.h>
#include "Network/network.h"
#include "Network/mqtt_trigger.h"
#include "Network/cmd_registry.h"
#include "Build/logger.h"
#include "Build/safeboot.h"
#include "Build/OTA/ota_pull.h"

// ── LED subsystem (single / multi-GPIO / PCA9685 RGB) ─────────────────────
// All LED hardware is coordinated through led.h regardless of which
// sub-modules (led_single_io, led_multi_io, led_pca) are compiled in.
// Feature flags in platformio.ini still control which hardware is active;
// led.cpp checks them internally rather than main.cpp scattering ifdefs.
// #include "LED/led.h"

// // ── Other subsystems ──────────────────────────────────────────────────────
// #ifdef FEATURE_SERVO
// #include "Motor/servos.h"
// #include "Motor/calibration.h"
// #endif

// #ifdef FEATURE_TESTER
// #include "Tester/tester.h"
// #endif

// #ifdef FEATURE_HRM
// #include "HRM/hrm.h"
// #endif

// ─────────────────────────────────────────────────────────────────────────────
// Increase the Arduino loop task stack for TLS (required by OTA pull).
// ─────────────────────────────────────────────────────────────────────────────
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

// ─────────────────────────────────────────────────────────────────────────────
// setup()
//
// Init order:
//   1. Serial + safeboot_check()  — reads RTC crash counter; if threshold hit,
//      only WiFi + OTA start and we return early.
//   2. network_init()             — WiFi + ArduinoOTA (push OTA).
//   3. mqtt_trigger_init()        — registers log sink + built-in commands.
//   4. servos_init()              — MUST precede led_init() when FEATURE_RGB is
//      active: led_pca shares the PCA9685 started here.
//   5. led_init()                 — initialises all enabled LED sub-modules.
//      led_register_commands()   — registers single "led:" prefix covering
//                                  led:single:, led:multi:, led:pca: routing.
//   6. calibration_init()         — TCP calibration server (servo only).
//   7. ota_pull_init()            — GitHub pull OTA; runs via loop().
//   8. tester_init()              — diagnostics; always last.
//   9. future later
// ─────────────────────────────────────────────────────────────────────────────
void setup()
{
    Serial.begin(115200);
    delay(200);

    // ── Crash-loop guard ───────────────────────────────────────────────
    bool safeMode = safeboot_check();
    LOG("Booting..." + String(safeMode ? " [SAFE MODE]" : ""));

    // ── WiFi + ArduinoOTA ─────────────────────────────────────────────
    network_init();

    if (safeMode)
    {
        // Safe mode: WiFi and ArduinoOTA are live.
        // MQTT also starts so "reset" and "ota" commands still work.
#ifdef FEATURE_MQTT
        mqtt_trigger_init();
#endif
        LOG("[SAFEBOOT] WiFi + OTA ready. Waiting for new firmware.");
        return;
    }

    // ── MQTT ───────────────────────────────────────────────────────────
#ifdef FEATURE_MQTT
    mqtt_trigger_init();
#endif

//     //  ── HRM ───────────────────────────────────────────────────────────
// #ifdef FEATURE_HRM
//     cmd_register("hrm:", [](const String &msg)
//                  {
//                      hrm_cmd(msg.substring(4)); // strip "hrm:" prefix before dispatching
//                  });
//     hrm_init();
// #endif

//     // ── Servos — must come BEFORE led_init() when FEATURE_RGB active ──
// #ifdef FEATURE_SERVO
//     servos_init();
//     servos_register_commands(); // registers "servo:"
//     calibration_init();
// #endif

//     // ── LED subsystem — single coord init + single command registration ─
//     // led_init() internally respects FEATURE_LED / FEATURE_GPIO_LEDS /
//     // FEATURE_RGB so there is no per-module ifdef needed here.
//     led_init();
//     led_register_commands(); // registers "led:" → routes led:single/multi/pca

    // ── Pull OTA ───────────────────────────────────────────────────────
#ifdef FEATURE_OTA_PULL
    ota_pull_init(OtaMode::AUTO);
#endif

//     // ── Diagnostics ────────────────────────────────────────────────────
// #ifdef FEATURE_TESTER
//     tester_init();
// #endif

    LOG("Ready.");
}

// ─────────────────────────────────────────────────────────────────────────────
// loop()
// ─────────────────────────────────────────────────────────────────────────────
void loop()
{
    network_handle();
    safeboot_update();

#ifdef FEATURE_MQTT
    mqtt_trigger_handle();
#endif

    if (safeboot_active()) return;

#ifdef FEATURE_OTA_PULL
    ota_pull_handle();
#endif

//     // Single call drives all LED sub-module loop() work
//     // (multi-IO breathe pattern + periodic re-scan).
//     led_handle();

// #ifdef FEATURE_SERVO
//     calibration_update();
// #endif

// #ifdef FEATURE_HRM
//     hrm_handle();
// #endif

// #ifdef FEATURE_TESTER
//     tester_handle();
// #endif
}
