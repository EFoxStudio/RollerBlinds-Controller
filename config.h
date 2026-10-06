#pragma once
#include <Arduino.h>

// Hardware
// GPIO20 is RX on C3 Super Mini, OK if USB CDC on boot is on
constexpr uint8_t PIN_RELAY_UP   = 20;
constexpr uint8_t PIN_RELAY_DOWN = 10;
constexpr uint8_t PIN_LED        = 8;

constexpr uint8_t RELAY_ON  = HIGH;
constexpr uint8_t RELAY_OFF = LOW;
constexpr uint8_t LED_ON    = LOW;   // C3 onboard LED is active-low
constexpr uint8_t LED_OFF   = HIGH;

static_assert(PIN_RELAY_UP != PIN_RELAY_DOWN, "Relay pins must differ");

// Location (Katowice)
constexpr double SITE_LAT_DEG = 50.2649;
constexpr double SITE_LON_DEG = 19.0238;

// Timezone & NTP
constexpr char TZ_POSIX[]            = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr int  TZ_STD_OFFSET_MIN     = 60;   // UTC+1
constexpr int  TZ_DST_EXTRA_MIN      = 60;   // UTC+2
constexpr char NTP_SERVER_1[]        = "pool.ntp.org";
constexpr char NTP_SERVER_2[]        = "time.nist.gov";
constexpr time_t MIN_VALID_EPOCH     = 1700000000;

// Timing / Motion
constexpr float    DEFAULT_TIME_UP_S         = 9.2f;
constexpr float    DEFAULT_TIME_DOWN_S       = 8.2f;
constexpr float    MIN_TRAVEL_TIME_S         = 1.0f;
constexpr float    MAX_TRAVEL_TIME_S         = 120.0f;
constexpr uint32_t MANUAL_MAX_MS             = 30000;
constexpr uint32_t MANUAL_PULSE_TIMEOUT_MS   = 800;   // Keep-alive timeout
constexpr uint32_t MOTOR_REVERSE_DEADTIME_MS = 400;   // Pause when swapping direction

// Delay limits
constexpr uint16_t MAX_DELAY_MIN = 240;

// Network
constexpr char     HOSTNAME[]                 = "blinds"; // http://blinds.local
constexpr uint32_t NETWORK_CHECK_PERIOD_MS    = 1000;
constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 15000;
constexpr uint16_t HTTP_PORT                  = 80;

#define WEB_AUTH_ENABLED 0 // 1 to enable HTTP auth (credentials in secrets.h)

// System loops & LED
constexpr uint32_t SCHEDULER_PERIOD_MS = 1000;
constexpr uint32_t LED_PERIOD_IDLE_MS  = 2000;
constexpr uint32_t LED_PERIOD_BUSY_MS  = 100;

// Logging
constexpr uint8_t LOG_CAPACITY = 30;
constexpr size_t  LOG_LINE_LEN = 96;

// Storage & Buffers
constexpr char NVS_NAMESPACE[] = "blinds";
constexpr size_t STATUS_JSON_RESERVE = 3072;