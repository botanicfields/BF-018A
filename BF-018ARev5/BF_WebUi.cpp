// BF-018A Rev.5
// web UI: status page, settings and serial log over HTTP
//
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include "BF_Log.h"
#include "BF_WebUi.h"
#include "BF_WebUiPage.h"

static WebServer server(80);
static const char *web_user("");
static const char *web_password("");

static const char *prefs_namespace = "jjysim";
static const char *auth_realm      = "JJY Simulator";
static const char *page_header     = "X-JJY";  // the page sends it with every change; a cross-site form can't
static const char *tz_chars        = "<>+-,./:";
static const char *ntp_chars       = ".-";

// restart after the response has reached the browser
enum pending_t {
  pending_none,
  pending_restart,
  pending_forget_wifi,
};
static pending_t     pending(pending_none);
static unsigned long pending_last_ms(0);
static const unsigned long pending_delay_ms(1000);

static char json_buffer[2048];
static char log_buffer[LogTee::m_size];

// Print into a fixed buffer, for building a JSON response
class BufferPrint : public Print {
 public:
  BufferPrint(char *buffer, size_t size) : m_buffer(buffer), m_size(size) { m_buffer[0] = '\0'; }
  size_t write(uint8_t c) override
  {
    if (m_length + 1 >= m_size) {
      m_overflow = true;
      return 0;
    }
    m_buffer[m_length++] = c;
    m_buffer[m_length] = '\0';
    return 1;
  }
  size_t Length()   { return m_length; }
  bool   Overflow() { return m_overflow; }

 private:
  char  *m_buffer;
  size_t m_size;
  size_t m_length = 0;
  bool   m_overflow = false;
};

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// POSIX TZ strings: std offset [dst [offset] [,rule,rule]], e.g. "CET-1CEST,M3.5.0,M10.5.0/3"
// The C library takes any text and quietly uses UTC for what it can't parse, so the web UI checks the syntax first.
// The page has the same check in JavaScript (validTz()).
static bool TzNumber(const char *&p, int min, int max)  // 1 to 3 digits
{
  int n(0), digits(0);
  while (isdigit((unsigned char)*p)) {
    if (++digits > 3) {
      return false;
    }
    n = n * 10 + (*p++ - '0');
  }
  return digits != 0 && n >= min && n <= max;
}

static bool TzName(const char *&p)  // 3 or more letters, or <...> with letters, digits, + and -
{
  const char *start = p;
  if (*p == '<') {
    ++p;
    while (isalnum((unsigned char)*p) || *p == '+' || *p == '-') {
      ++p;
    }
    if (*p != '>' || p - start - 1 < 3) {
      return false;
    }
    ++p;
    return true;
  }
  while (isalpha((unsigned char)*p)) {
    ++p;
  }
  return p - start >= 3;
}

static bool TzTime(const char *&p, bool signed_time, int max_hours)  // [+-]hh[:mm[:ss]]
{
  if (signed_time && (*p == '+' || *p == '-')) {
    ++p;
  }
  if (!TzNumber(p, 0, max_hours)) {
    return false;
  }
  for (int i = 0; i < 2 && *p == ':'; ++i) {
    ++p;
    if (!TzNumber(p, 0, 59)) {
      return false;
    }
  }
  return true;
}

static bool TzRule(const char *&p)  // Mm.w.d, Jn or n, then an optional /time
{
  if (*p == 'M') {
    ++p;
    if (!TzNumber(p, 1, 12) || *p++ != '.' || !TzNumber(p, 1, 5) || *p++ != '.' || !TzNumber(p, 0, 6)) {
      return false;
    }
  }
  else if (*p == 'J') {
    ++p;
    if (!TzNumber(p, 1, 365)) {
      return false;
    }
  }
  else if (!TzNumber(p, 0, 365)) {
    return false;
  }
  if (*p == '/') {
    ++p;
    return TzTime(p, false, 167);
  }
  return true;
}

bool WebUiValidTimeZone(const char *tz)
{
  const char *p = tz;
  if (!TzName(p) || !TzTime(p, true, 24)) {
    return false;
  }
  if (*p == '\0') {
    return true;  // no daylight saving time
  }
  if (!TzName(p)) {
    return false;
  }
  if (*p != '\0' && *p != ',' && !TzTime(p, true, 24)) {
    return false;
  }
  if (*p == '\0') {
    return true;  // the C library's default rules
  }
  if (*p++ != ',' || !TzRule(p) || *p++ != ',' || !TzRule(p)) {
    return false;
  }
  return *p == '\0';
}

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// settings
static bool ValidResolution(uint32_t bits)
{
  return bits == 8 || bits == 2 || bits == 1;
}

static bool ValidText(const String &s, size_t max_length, const char *extra_chars)
{
  if (s.length() == 0 || s.length() >= max_length) {
    return false;
  }
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == '\0' || (!isalnum((unsigned char)c) && strchr(extra_chars, c) == nullptr)) {
      return false;
    }
  }
  return true;
}

void WebUiLoadSettings(WebUiSettings *settings)
{
  Preferences prefs;
  if (!prefs.begin(prefs_namespace, true)) {
    return;  // nothing stored yet
  }
  uint32_t frequency = prefs.getUInt("freq", settings->jjy_frequency);
  if (frequency == 40000 || frequency == 60000) {
    settings->jjy_frequency = frequency;
  }
  uint8_t resolution = prefs.getUChar("res", settings->carrier_resolution);
  if (ValidResolution(resolution)) {
    settings->carrier_resolution = resolution;
  }
  String time_zone = prefs.getString("tz", settings->time_zone);
  if (ValidText(time_zone, sizeof(settings->time_zone), tz_chars) && WebUiValidTimeZone(time_zone.c_str())) {
    strlcpy(settings->time_zone, time_zone.c_str(), sizeof(settings->time_zone));
  }
  String ntp_server = prefs.getString("ntp", settings->ntp_server);
  if (ValidText(ntp_server, sizeof(settings->ntp_server), ntp_chars)) {
    strlcpy(settings->ntp_server, ntp_server.c_str(), sizeof(settings->ntp_server));
  }
  prefs.end();
}

static bool SaveSettings(uint32_t frequency, uint8_t resolution, const String &time_zone, const String &ntp_server)
{
  Preferences prefs;
  if (!prefs.begin(prefs_namespace, false)) {
    return false;
  }
  bool saved = prefs.putUInt("freq", frequency) == sizeof(frequency)
            && prefs.putUChar("res", resolution) == sizeof(resolution)
            && prefs.putString("tz", time_zone) == time_zone.length()
            && prefs.putString("ntp", ntp_server) == ntp_server.length();
  prefs.end();
  return saved;
}

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// HTTP handlers
static void SendJson(int code, const char *json)
{
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(code, "application/json", json);
}

// for requests that change something: password set, sent by the page, and the password given
static bool Allowed()
{
  if (!WebUiAuthEnabled()) {
    SendJson(403, "{\"ok\":false,\"error\":\"read-only\"}");
    return false;
  }
  if (!server.hasHeader(page_header)) {
    SendJson(403, "{\"ok\":false,\"error\":\"forbidden\"}");
    return false;
  }
  if (!server.authenticate(web_user, web_password)) {
    server.requestAuthentication(DIGEST_AUTH, auth_realm, "{\"ok\":false,\"error\":\"password\"}");
    return false;
  }
  return true;
}

static void Schedule(pending_t what)
{
  pending = what;
  pending_last_ms = millis();
}

static void HandlePage()
{
  server.sendHeader("Cache-Control", "no-cache");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.sendHeader("Content-Security-Policy",
                    "default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; img-src 'self' data:; "
                    "connect-src 'self'; frame-ancestors 'none'; form-action 'none'; base-uri 'none'");
  server.send_P(200, "text/html; charset=utf-8", web_ui_page);
}

static void HandleStatus()
{
  BufferPrint out(json_buffer, sizeof(json_buffer));
  WebUiStatusJson(out);
  if (out.Overflow()) {
    SendJson(500, "{\"ok\":false,\"error\":\"status too long\"}");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "application/json", json_buffer, out.Length());
}

static void HandleLog()
{
  uint32_t from = strtoul(server.arg("from").c_str(), nullptr, 10);
  uint32_t first(0);
  size_t length = Log.Read(from, log_buffer, sizeof(log_buffer), &first);
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Log-First", String(first));
  server.sendHeader("X-Log-Next", String(first + length));
  server.send_P(200, "text/plain; charset=utf-8", log_buffer, length);
}

static void HandleSettings()
{
  if (!Allowed()) {
    return;
  }
  uint32_t frequency  = server.arg("freq").toInt();
  uint32_t resolution = server.arg("res").toInt();
  String time_zone    = server.arg("tz");
  String ntp_server  = server.arg("ntp");
  time_zone.trim();
  ntp_server.trim();
  if ((frequency != 40000 && frequency != 60000) || !ValidResolution(resolution) ||
      !ValidText(time_zone, sizeof(WebUiSettings::time_zone), tz_chars) || !WebUiValidTimeZone(time_zone.c_str()) ||
      !ValidText(ntp_server, sizeof(WebUiSettings::ntp_server), ntp_chars)) {
    SendJson(400, "{\"ok\":false,\"error\":\"invalid\"}");
    return;
  }
  if (!SaveSettings(frequency, resolution, time_zone, ntp_server)) {
    SendJson(500, "{\"ok\":false,\"error\":\"not saved\"}");
    return;
  }
  Log.printf("[Web]settings saved: %lu Hz, %lu bits, %s, %s: restarting..\n", (unsigned long)frequency, (unsigned long)resolution,
             time_zone.c_str(), ntp_server.c_str());
  SendJson(200, "{\"ok\":true,\"restart\":true}");
  Schedule(pending_restart);
}

static void HandleLed()
{
  if (!Allowed()) {
    return;
  }
  bool enable = server.arg("on") == "1";
  WebUiSetLed(enable);
  Log.printf("[Web]LED monitor %s\n", enable ? "on" : "off");
  SendJson(200, "{\"ok\":true}");
}

static void HandleSync()
{
  if (!Allowed()) {
    return;
  }
  Log.print("[Web]time sync requested\n");
  WebUiSyncTime();
  SendJson(200, "{\"ok\":true}");
}

static void HandleRestart()
{
  if (!Allowed()) {
    return;
  }
  Log.print("[Web]restart requested: restarting..\n");
  SendJson(200, "{\"ok\":true,\"restart\":true}");
  Schedule(pending_restart);
}

static void HandleForgetWifi()
{
  if (!Allowed()) {
    return;
  }
  Log.print("[Web]erase SSID/Key requested: restarting..\n");
  SendJson(200, "{\"ok\":true,\"restart\":true}");
  Schedule(pending_forget_wifi);
}

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// web UI
void WebUiBegin(const char *hostname, const char *user, const char *password)
{
  web_user = user;
  web_password = password;

  const char *headers[] = { page_header };
  server.collectHeaders(headers, 1);
  server.on("/",                HTTP_GET,  HandlePage);
  server.on("/api/status",      HTTP_GET,  HandleStatus);
  server.on("/api/log",         HTTP_GET,  HandleLog);
  server.on("/api/settings",    HTTP_POST, HandleSettings);
  server.on("/api/led",         HTTP_POST, HandleLed);
  server.on("/api/sync",        HTTP_POST, HandleSync);
  server.on("/api/restart",     HTTP_POST, HandleRestart);
  server.on("/api/forget-wifi", HTTP_POST, HandleForgetWifi);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found\n"); });
  server.begin();

  if (MDNS.begin(hostname)) {
    MDNS.addService("http", "tcp", 80);
  }
  Log.printf("[Web]http://%s.local/ or http://%s/ (%s)\n", hostname, WiFi.localIP().toString().c_str(),
             WebUiAuthEnabled() ? "changes need the password" : "read-only: no password set");
}

void WebUiHandle()
{
  server.handleClient();

  if (pending != pending_none && millis() - pending_last_ms >= pending_delay_ms) {
    if (pending == pending_forget_wifi) {
      WiFiManager wm;
      wm.resetSettings();  // as holding the button at reset does: the config portal starts after the restart
    }
    ESP.restart();
  }
}

bool WebUiAuthEnabled()
{
  return web_password != nullptr && web_password[0] != '\0';
}

void WebUiJsonString(Print &out, const char *s)
{
  out.print('"');
  for (; *s != '\0'; ++s) {
    unsigned char c = *s;
    if (c == '"' || c == '\\') {
      out.print('\\');
      out.print((char)c);
    }
    else if (c < 0x20) {
      out.printf("\\u%04x", c);
    }
    else {
      out.print((char)c);
    }
  }
  out.print('"');
}
