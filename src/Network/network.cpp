#include "network.h"
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "../Build/secrets.h"
#include "../Build/logger.h"

// Credential arrays are defined in secrets.h (gitignored).
// Format: { {"SSID", "password"}, ... }
static const char *networks[][2] = WIFI_NETWORKS;

static bool otaInProgress = false;

static void connectWiFi()
{
    for (auto &net : networks)
    {
        Serial.printf("Trying %s...\n", net[0]);
        WiFi.begin(net[0], net[1]);

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20)
        {
            delay(500);
            Serial.print(".");
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            LOGF("\nConnected to %s, IP: %s\n",
                          net[0], WiFi.localIP().toString().c_str());
            return;
        }

        WiFi.disconnect();
        LOG("\nFailed, trying next...");
    }
    LOG("All networks failed");
}

static void setupOTA()
{
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]()
    {
        otaInProgress = true;
        LOG("OTA starting...");
    });
    ArduinoOTA.onEnd([]()
    {
        otaInProgress = false;
        LOG("OTA done, rebooting...");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
        Serial.printf("OTA: %u%%\n", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error)
    {
        otaInProgress = false;
        Serial.printf("OTA Error: %u\n", error);
    });

    ArduinoOTA.begin();
    LOG("OTA ready");
}

void network_init()
{
    connectWiFi();
    setupOTA();
}

void network_handle()
{
    ArduinoOTA.handle();
}

bool network_ota_in_progress()
{
    return otaInProgress;
}
