#pragma once

#include <stdint.h>

// GPIO numbers, not physical header positions. RC522 runs at 3.3 V.
constexpr uint8_t PIN_SS = 10;    // RC522 SDA / SS
constexpr uint8_t PIN_MOSI = 11;
constexpr uint8_t PIN_SCK = 12;
constexpr uint8_t PIN_MISO = 13;
constexpr uint8_t PIN_RST = 14;   // RC522 RST, NOT the ESP32 RST pin

// One KNOWN key per source sector. FF...FF is the factory key, not a bypass.
// Change only the rows for which you have the actual key.
constexpr uint8_t SOURCE_KEYS[16][6] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // sector 0
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // sector 1
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // sector 2
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // sector 3
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // sector 4
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // sector 5
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // sector 6
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // sector 7
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // sector 8
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // sector 9
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // sector 10
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // sector 11
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // sector 12
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // sector 13
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // sector 14
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // sector 15
};
// false = authenticate with Key A; true = authenticate with Key B.
constexpr bool SOURCE_USE_KEY_B[16] = {};

// Restoration is for a separate blank M1/S50 card with factory Key A.
constexpr uint8_t TARGET_KEY[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
constexpr uint32_t CARD_TIMEOUT_MS = 15000;
constexpr uint32_t CONFIRM_TIMEOUT_MS = 30000;

// Standalone 2.4 GHz access point. No router or Internet connection is needed.
constexpr char WIFI_AP_SSID[] = "ICCard-S3";
constexpr char WIFI_AP_PASSWORD[] = "iccard2026"; // At least 8 characters.
static_assert(sizeof(WIFI_AP_PASSWORD) >= 9 && sizeof(WIFI_AP_PASSWORD) <= 64,
              "Wi-Fi password must contain 8 to 63 characters");
