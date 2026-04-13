#include "ota_pull.h"
#include "../certs.h"
#include "../secrets.h"
#include "../logger.h"
#include "../config.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <WiFi.h>

// Build URLs from config so only config.h needs editing for a repo change.
#define OTA_HASH_URL \
    "https://raw.githubusercontent.com/" \
    CFG_OTA_REPO_OWNER "/" CFG_OTA_REPO_NAME "/" CFG_OTA_BRANCH "/firmware.md5"

#define OTA_BIN_URL \
    "https://raw.githubusercontent.com/" \
    CFG_OTA_REPO_OWNER "/" CFG_OTA_REPO_NAME "/" CFG_OTA_BRANCH "/firmware.bin"

// ── State ────────────────────────────────────────────────────────────────────
static volatile bool     otaRunning     = false;
static volatile uint32_t lastCheck      = 0;
static volatile bool     forceCheck     = false;
static bool              lastWasUpToDate = false;
static OtaMode           otaMode        = OtaMode::AUTO;

bool ota_pull_in_progress() { return otaRunning; }

// ── Shared helper — authenticated TLS request ─────────────────────────────
static bool beginSecureRequest(WiFiClientSecure &client,
                               HTTPClient       &http,
                               const char       *url,
                               int               timeoutMs)
{
    client.setInsecure(); // cert pinning via GITHUB_ROOT_CA is opt-in
    if (!http.begin(client, url))
        return false;

    http.addHeader("Authorization", "Bearer " + String(GITHUB_TOKEN));
    http.addHeader("Accept",        "application/vnd.github.raw");
    http.setTimeout(timeoutMs);
    return true;
}

// ── FreeRTOS task — never touches the main loop thread ───────────────────
static void otaTask(void *)
{
    if (ESP.getFreeHeap() < CFG_OTA_MIN_HEAP || WiFi.status() != WL_CONNECTED)
    {
        otaRunning = false;
        vTaskDelete(nullptr);
        return;
    }

    // 1. Fetch remote MD5 ───────────────────────────────────────────────────
    WiFiClientSecure hashClient;
    HTTPClient       hashHttp;

    if (!beginSecureRequest(hashClient, hashHttp, OTA_HASH_URL, CFG_OTA_HASH_TIMEOUT))
    {
        LOG("[OTA] Failed to begin hash request");
        otaRunning = false;
        vTaskDelete(nullptr);
        return;
    }

    int code = hashHttp.GET();
    if (code != 200)
    {
        LOGF("[OTA] Hash fetch failed: HTTP %d\n", code);
        hashHttp.end();
        otaRunning = false;
        vTaskDelete(nullptr);
        return;
    }

    String remoteMD5 = hashHttp.getString();
    remoteMD5.trim();
    hashHttp.end();

    // 2. Compare against running sketch ────────────────────────────────────
    String localMD5 = ESP.getSketchMD5();

    if (localMD5 == remoteMD5)
    {
        if (!lastWasUpToDate)
        {
            LOG("[OTA] Up to date");
            lastWasUpToDate = true;
        }
        otaRunning = false;
        vTaskDelete(nullptr);
        return;
    }

    lastWasUpToDate = false;
    LOG("[OTA] Local:  " + localMD5);
    LOG("[OTA] Remote: " + remoteMD5);
    LOG("[OTA] New firmware detected, downloading...");

    // 3. Download and flash ────────────────────────────────────────────────
    WiFiClientSecure binClient;
    HTTPClient       binHttp;

    if (!beginSecureRequest(binClient, binHttp, OTA_BIN_URL, CFG_OTA_DL_TIMEOUT))
    {
        LOG("[OTA] Failed to begin download request");
        otaRunning = false;
        vTaskDelete(nullptr);
        return;
    }

    code = binHttp.GET();
    if (code != 200)
    {
        LOGF("[OTA] Download failed: HTTP %d\n", code);
        binHttp.end();
        otaRunning = false;
        vTaskDelete(nullptr);
        return;
    }

    int totalSize = binHttp.getSize();
    if (totalSize <= 0)
    {
        LOG("[OTA] Invalid content-length");
        binHttp.end();
        otaRunning = false;
        vTaskDelete(nullptr);
        return;
    }

    if (!Update.begin(totalSize))
    {
        LOG("[OTA] Not enough flash space");
        Update.printError(Serial);
        binHttp.end();
        otaRunning = false;
        vTaskDelete(nullptr);
        return;
    }

    size_t written = Update.writeStream(*binHttp.getStreamPtr());

    if (written != (size_t)totalSize)
    {
        LOGF("[OTA] Size mismatch: wrote %u of %d\n", written, totalSize);
        binHttp.end();
        otaRunning = false;
        vTaskDelete(nullptr);
        return;
    }

    if (Update.end() && Update.isFinished())
    {
        LOGF("[OTA] Flashed %u bytes — rebooting\n", written);
        binHttp.end();
        delay(500);
        ESP.restart();
    }
    else
    {
        LOG("[OTA] Flash failed");
        Update.printError(Serial);
    }

    binHttp.end();
    otaRunning = false;
    vTaskDelete(nullptr);
}

// ── Public API ────────────────────────────────────────────────────────────
void ota_pull_init(OtaMode mode)
{
    otaMode    = mode;
    lastCheck  = 0;
    forceCheck = false;
    otaRunning = false;
    LOGF("[OTA] Pull OTA ready — %s mode\n",
         mode == OtaMode::AUTO ? "AUTO" : "TIMER");
}

void ota_pull_force()
{
    if (otaRunning) return;
    forceCheck = true;
    LOG("[OTA] Force check scheduled");
}

void ota_pull_handle()
{
    if (otaRunning) return;

    bool shouldRun = false;

    if (forceCheck)
    {
        forceCheck = false;
        shouldRun  = true;
    }
    else if (otaMode == OtaMode::TIMER)
    {
        shouldRun = ((millis() - lastCheck) > CFG_OTA_CHECK_MS);
    }
    // AUTO: only forceCheck triggers, no interval

    if (shouldRun)
    {
        lastCheck  = millis();
        otaRunning = true;
        if (xTaskCreate(otaTask, "ota_pull",
                        CFG_OTA_TASK_STACK, nullptr, 1, nullptr) != pdPASS)
        {
            LOG("[OTA] Task creation failed");
            otaRunning = false;
        }
    }
}
