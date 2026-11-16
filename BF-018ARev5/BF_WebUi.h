// BF-018A Rev.5
// web UI: status page, settings and serial log over HTTP
//
#pragma once
#include <Arduino.h>

// settings the web UI can change: stored in NVS and applied at boot
struct WebUiSettings {
  uint32_t jjy_frequency;   // 40000 or 60000
  char     time_zone[64];   // POSIX TZ string, e.g. "JST-9"
  char     ntp_server[64];  // host name or IPv4 address
  uint8_t  carrier_resolution;  // LEDC duty resolution in bits: 8, 2 or 1; fewer bits, a 60kHz carrier closer to 60kHz
};

void WebUiLoadSettings(WebUiSettings *settings);  // keeps the given values for anything not stored
void WebUiBegin(const char *hostname, const char *user, const char *password);
void WebUiHandle();       // call from loop()
bool WebUiAuthEnabled();  // a password is set, so the web UI may change things
bool WebUiValidTimeZone(const char *tz);  // a POSIX TZ string the C library can use, e.g. "JST-9"
void WebUiJsonString(Print &out, const char *s);  // write s as a JSON string

// implemented by the sketch
void WebUiStatusJson(Print &out);  // the object /api/status returns
void WebUiSetLed(bool enable);     // LED monitor on/off, like the button
void WebUiSyncTime();              // ask the NTP server for the time now
