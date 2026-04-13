#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// ota_pull.h — pull-OTA subsystem.
//
// Runs firmware downloads on a dedicated FreeRTOS task so loop() never
// blocks.  Two modes:
//
//   TIMER — checks GitHub every CFG_OTA_CHECK_MS AND on ota_pull_force().
//   AUTO  — only checks when ota_pull_force() is called (e.g. via MQTT).
// ─────────────────────────────────────────────────────────────────────────────

enum class OtaMode
{
    TIMER, // scheduled interval + force trigger
    AUTO   // force trigger only, no interval
};

// Call once from setup().
void ota_pull_init(OtaMode mode = OtaMode::TIMER);

// Call from loop(). Schedules background task when appropriate.
// Never blocks — download runs on a separate FreeRTOS task.
void ota_pull_handle();

// Returns true while a firmware download/flash is in progress.
// Gate any operations (servos, TCP) that must not run during a flash.
bool ota_pull_in_progress();

// Trigger an immediate check regardless of mode or interval.
// Safe to call from MQTT callback or TCP command handler.
void ota_pull_force();
