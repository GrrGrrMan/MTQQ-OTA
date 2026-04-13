#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// config.h — single source of truth for all tunable values.
// Hardware pins, intervals, topic strings, and limits all live here.
// Never put bare literals in .cpp files — reference a CFG_ constant instead.
// ─────────────────────────────────────────────────────────────────────────────



// ── LED / LEDC ───────────────────────────────────────────────────────────────
// #define CFG_LED_PIN 2
// #define CFG_LED_FREQ_HZ 5000
// #define CFG_LED_RESOLUTION 8 // bits  → 0-255 duty range
// #define CFG_LED_TIMER 0      // LEDC_TIMER_0
// #define CFG_LED_CHANNEL 0    // LEDC_CHANNEL_0
// #define CFG_LED_STOP_FADE_MS 500
// #define CFG_LED_LOOP_MAX_CMDS 16
// #define CFG_LED_LOOP_CMD_LEN 32
// #define CFG_RGB_LED_COUNT 4
// #define CFG_RGB_START_CHANNEL 0    // PCA9685 ch 0-11 used for LEDs
// #define CFG_RGB_COMMON_ANODE false // true = invert PWM duty
// #define CFG_RGB_FADE_STEPS 50      // smoothness of software fades



// ── Single LED — software fade engine ────────────────────────────────────────
// #define CFG_LED_FADE_TICK_MS 10 // engine tick rate (~100 Hz)
// #define CFG_LED_TIMER_MAX 4     // max concurrent one-shot timers



// ── GPIO Multi-LED ───────────────────────────────────────────────────────────
// Candidate GPIO pins to scan for LEDs at boot (and periodically in AUTO mode).
// Edit this list to match your wiring.  Pins already used by other peripherals
// (camera, I2C, UART, onboard LED) should NOT be included.
// #define CFG_GPIO_LED_PINS {13}
// #define CFG_GPIO_LED_MAX_PINS 8 // max entries in active-pin table



// // LEDC config for GPIO LEDs.  Channels start here (must not overlap with
// // CFG_LED_CHANNEL = 0 used by the single-LED module).
// // Uses the same LEDC timer as the single-LED module (timer 0).
// #define CFG_GPIO_LED_START_CHANNEL 1 // channels 1..1+MAX_PINS-1
// #define CFG_GPIO_LED_LEDC_TIMER 0    // share LEDC_TIMER_0
// #define CFG_GPIO_LED_FREQ_HZ 5000

// Detection: ADC reading (mV) above this threshold → LED-to-VCC circuit found.
// Lower if you're missing real LEDs; raise if floating pins false-trigger.
// #define CFG_GPIO_LED_DETECT_MV 1800

// // AUTO mode: re-scan candidate pins every N ms.
// #define CFG_GPIO_LED_SCAN_INTERVAL_MS 86400000UL // disabled ufn

// // AUTO mode breathe pattern timing.
// #define CFG_GPIO_LED_BREATHE_STEP_MS 20 // ms per sine step (~50 Hz)
// #define CFG_GPIO_LED_BREATHE_INC 0.05f  // radians per step (~3 s period)

// // ── Servo / PCA9685 ──────────────────────────────────────────────────────────
// #define CFG_I2C_SDA 14
// #define CFG_I2C_SCL 15
// #define CFG_PCA9685_ADDR 0x40
// #define CFG_SERVO_FREQ_HZ 50
// #define CFG_SERVO_MIN 150     // pulse count @ 0 deg
// #define CFG_SERVO_MAX 600     // pulse count @ 180 deg
// #define CFG_SERVO_CHANNELS 16 // 0-15

// // ── Calibration TCP server ───────────────────────────────────────────────────
// #define CFG_CALIB_PORT 1234
// #define CFG_ANGLE_STEP 5 // degrees per key press

// ── MQTT ─────────────────────────────────────────────────────────────────────
#define CFG_MQTT_HOST "broker.hivemq.com"
#define CFG_MQTT_PORT 1883
#define CFG_MQTT_CLIENT "esp32-spiderbot"
#define CFG_MQTT_TOPIC_OTA "xyzesp/ota/" // espname/module/... (can just be any name)
#define CFG_MQTT_TOPIC_CMD "xyzesp/cmd"
#define CFG_MQTT_TOPIC_LOG "xyzesp/log"
#define CFG_MQTT_MAX_MSG 256
#define CFG_MQTT_BUFFER_SIZE 512
#define CFG_MQTT_RETRY_MS 10000 // reconnect back-off

// // ── HRM (Heart Rate Monitor) ──────────────────────────────────────────────────

// // Hardware — GPIO4 is the flash LED on the AI-Thinker ESP32-CAM.
// // LEDC channel 6 + timer 2 are free (0=single-LED, 1-4=multi-IO, 5=camera XCLK).
// #define CFG_HRM_LED_PIN 4     // Flash LED pin
// #define CFG_HRM_LED_CHANNEL 6 // LEDC channel (must not clash with others)
// #define CFG_HRM_LED_TIMER 2   // LEDC timer
// #define CFG_HRM_LED_DUTY 80   // Starting brightness 0-255.
//                               //   Too bright (>150) saturates camera.
//                               //   Too dim (<30) → poor SNR.
//                               //   Tune with  hrm:led:<n>  over MQTT.

// // Signal processing
// #define CFG_HRM_SAMPLE_RATE 30  // Hz — camera capture rate
// #define CFG_HRM_BUFFER_SIZE 150 // samples (5 seconds at 30 Hz)
// #define CFG_HRM_CAL_MS 5000UL   // ms before peak detection starts
// #define CFG_HRM_MIN_BPM 40      // reject peaks faster than this
// #define CFG_HRM_MAX_BPM 200     // reject peaks slower than this

// // FreeRTOS task — camera + DSP need more stack than most tasks
// #define CFG_HRM_TASK_STACK 10240 // bytes

// // MQTT topics
// #define CFG_HRM_TOPIC_RAW "beanspiderbot/hrm/raw"       // filtered signal float
// #define CFG_HRM_TOPIC_BPM "beanspiderbot/hrm/bpm"       // integer BPM
// #define CFG_HRM_TOPIC_STATUS "beanspiderbot/hrm/status" // state string

// ── Safe Boot ────────────────────────────────────────────────────────────────
// Enter safe mode (WiFi + OTA only) after this many consecutive panic resets.
#define CFG_SAFEBOOT_THRESHOLD 3
// Clear the crash counter once the device has run stably for this long (ms).
#define CFG_SAFEBOOT_STABLE_MS 30000UL // 30 seconds

// ── OTA Pull ─────────────────────────────────────────────────────────────────
#define CFG_OTA_REPO_OWNER "github-user"
#define CFG_OTA_REPO_NAME "esp-example-repo"
#define CFG_OTA_BRANCH "main"
#define CFG_OTA_CHECK_MS 18000000UL // 5 hours between auto-checks
#define CFG_OTA_MIN_HEAP 60000      // bytes — skip OTA if heap below this
#define CFG_OTA_TASK_STACK 20480    // FreeRTOS task stack in bytes
#define CFG_OTA_HASH_TIMEOUT 10000  // ms for MD5 fetch
#define CFG_OTA_DL_TIMEOUT 30000    // ms for firmware download