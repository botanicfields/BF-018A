// copyright 2025 BotanicFields, Inc.
// BF-018A Rev.5
// JJY Simulator for ATOM Lite / ATOM Matrix / ATOMS3 Lite, with a web UI
//
#include <M5Unified.h>
#include <FastLED.h>      // https://github.com/FastLED/FastLED
#include <Ticker.h>
#include <WiFi.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include <esp_timer.h>
#include "BF_Log.h"
#include "BF_Ntp.h"
#include "BF_WebUi.h"

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// settings: the defaults below are used until they are changed on the web UI
WebUiSettings settings = {
  40000,           // jjy_frequency: 40kHz(east), 60kHz(west)
  "JST-9",         // time_zone
  "pool.ntp.org",  // ntp_server
  8,               // carrier_resolution: LEDC duty resolution in bits, 8, 2 or 1 (see TcoInit())
};

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// for the web UI: http://jjy-sim.local/
const char* web_hostname = "jjy-sim";
const char* web_user     = "admin";
const char* web_password = "";  // set a password to allow changes from the web UI; empty: the page is read-only

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// for TCO(Time Code Output)
struct tm       td;  // time of day: year, month, day, day of week, hour, minute, second
struct timespec ts;  // time spec: second, nano-second

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// for NTP
volatile bool localtime_valid(false);  // set by loop() at the first SNTP sync, read by the Ticker
time_t ntp_last_sync(0);  // when SNTP last set the time, 0: not yet

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// for WiFi
const          int wifi_config_portal_timeout_sec(60);
const unsigned int wifi_retry_interval_ms(60000);
      unsigned int wifi_retry_last_ms(0);
const          int wifi_retry_max_times(3);
               int wifi_retry_times(0);

wl_status_t wifi_status(WL_NO_SHIELD);
const char* wl_status_str[] = {
  "WL_IDLE_STATUS",      // 0
  "WL_NO_SSID_AVAIL",    // 1
  "WL_SCAN_COMPLETED",   // 2
  "WL_CONNECTED",        // 3
  "WL_CONNECT_FAILED",   // 4
  "WL_CONNECTION_LOST",  // 5
  "WL_DISCONNECTED",     // 6
  "WL_NO_SHIELD",        // 7 <-- 255
  "wl_status invalid",   // 8
};

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// for FastLED
const int led_pin_atom(27);        // GPIO27 for ATOM Lite/Matrix
const int led_pin_atoms3lite(35);  // GPIO35 for ATOMS3 Lite
const unsigned char led_num(25);   // ATOM Matrix: 25, ATOM lite/ATOMS3 Lite: 1 --> 25
CRGB leds[led_num]; 

const int led_on_r(0x80);                // 0..0xFF
const int led_on_g(0x80);                // 0..0xFF
const int led_on_b(0x80);                // 0..0xFF
const int led_brightness(40);            // 0..0xFF
const unsigned int blink_slow_ms(1000);  // blink period
const unsigned int blink_fast_ms( 200);  // blink period

enum led_r_t {
  led_r_off,   // WL_CONNECTED
  led_r_slow,  // WL_NO_SSID_AVAIL
  led_r_fast,  // WL_DISCONNECTED
  led_r_on,    // WL_IDLE_STATUS, WL_CONNECTION_LOST, etc.
};
enum led_g_t {
  led_g_off,   // time valid
  led_g_slow,  // waiting time valid
  led_g_fast,  //
  led_g_on,    // WiFi configuration portal is active
};
enum led_b_t {
  led_b_off,   // TCO off
  led_b_slow,  //
  led_b_fast,  //
  led_b_on,    // TCO on
};
led_r_t led_r(led_r_off);
led_g_t led_g(led_g_off);
led_b_t led_b(led_b_off);

bool led_enable(true);

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// LED
void LedShow()
{
  CRGB led(0);
  if (led_enable) {
    switch (led_r) {
      case led_r_on  : led.r = led_on_r;  break;
      case led_r_fast: led.r = LedBlink(blink_fast_ms) ? led_on_r : 0;  break;
      case led_r_slow: led.r = LedBlink(blink_slow_ms) ? led_on_r : 0;  break;
      default:  break;  // led.r = 0
    }
    switch (led_g) {
      case led_g_on  : led.g = led_on_g;  break;
      case led_g_fast: led.g = LedBlink(blink_fast_ms) ? led_on_g : 0;  break;
      case led_g_slow: led.g = LedBlink(blink_slow_ms) ? led_on_g : 0;  break;
      default:  break;  // led.g = 0
    }
    switch (led_b) {
      case led_b_on  : led.b = led_on_b;  break;
      case led_b_fast: led.b = LedBlink(blink_fast_ms) ? led_on_b : 0;  break;
      case led_b_slow: led.b = LedBlink(blink_slow_ms) ? led_on_b : 0;  break;
      default:  break;  // led.b = 0
    }
  }
  leds[0] = led;
  FastLED.show();
}

bool LedBlink(unsigned int period_ms)
{
  return millis() / period_ms % 2 != 0;
}

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// WiFi
const char* WlStatus(wl_status_t wl_status)
{
  if (wl_status >= 0 && wl_status <= 6) {
    return wl_status_str[wl_status];
  }
  if (wl_status == 255) {
    return wl_status_str[7];
  }
  return wl_status_str[8];
}

void WifiCheck()
{
  wl_status_t wifi_status_new = WiFi.status();
  if (wifi_status != wifi_status_new) {
    wifi_status = wifi_status_new;
    Log.printf("[WiFi]%s\n", WlStatus(wifi_status));
    switch (wifi_status) {
      case WL_CONNECTED    : led_r = led_r_off;   break;
      case WL_NO_SSID_AVAIL: led_r = led_r_slow;  break;
      case WL_DISCONNECTED : led_r = led_r_fast;  break;
      default              : led_r = led_r_on;    break;  // state transition also
    }
  }

  // retry interval
  if (millis() - wifi_retry_last_ms < wifi_retry_interval_ms) {
    return;
  }
  wifi_retry_last_ms = millis();

  // reboot if wifi connection fails
  if (wifi_status == WL_CONNECT_FAILED) {
    Log.print("[WiFi]connect failed: rebooting..\n");
    ESP.restart();
    return;
  }

  // let the wifi process do if wifi is not disconnected
  if (wifi_status != WL_DISCONNECTED) {
    wifi_retry_times = 0;
    return;
  }

  // reboot if wifi is disconnected for a long time
  if (++wifi_retry_times > wifi_retry_max_times) {
    Log.print("[WiFi]disconnect timeout: rebooting..\n");
    ESP.restart();
    return;
  }

  // reconnect, and reboot if reconnection fails
  Log.printf("[WiFi]reconnect %d\n", wifi_retry_times);
  if (!WiFi.reconnect()) {
    Log.print("[WiFi]reconnect failed: rebooting..\n");
    ESP.restart();
    return;
  };
}

void WifiConfigModeCallback(WiFiManager *wm)
{
  led_g = led_g_on;  // green LED indicates configuration portal is active
  LedShow();
}

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// TCO(Time Code Output)
Ticker tk;
const int ticker_interval_ms(10);  // poll every 10ms, TcoGen() acts once per 100ms slot
const int marker(0xff);  // marker code TcoValue() returns

// PWM for TCO signal
const uint8_t  ledc_pin_atom(22);   // GPIO22 for ATOM Lite/Matrix
const uint8_t  ledc_pin_atoms3(5);  // GPIO5 for ATOMS3 Lite
const uint8_t  ledc_channel(0);     // board package 2.x drives LEDC by channel
const uint32_t ledc_duty_off(0);    // 0
      uint32_t ledc_duty_on(128);   // 50%: half of 2^settings.carrier_resolution, set by TcoInit()
      uint8_t  ledc_pin(ledc_pin_atom);
      uint32_t ledc_frequency(0);   // the carrier LEDC makes: close to settings.jjy_frequency

// statistics of the 100ms slots, copied once a minute for the web UI
struct tco_stats_t {
  int    count;
  int    distribution[9];
  double average;
  double std_deviation;
  int    min;
  int    max;
};
tco_stats_t  tco_stats = {};
portMUX_TYPE tco_stats_mux = portMUX_INITIALIZER_UNLOCKED;

void TcoInit()
{
  if (M5.getBoard() == m5::board_t::board_M5AtomS3Lite) {
    ledc_pin = ledc_pin_atoms3;
  }

  // The LEDC timer divides its 80MHz clock by a divider with 8 fractional bits, and counts 2^resolution steps per
  // carrier cycle. 40kHz comes out exact at any resolution. 60kHz does not: at 8 bits the nearest divider gives
  // 60015Hz (+250ppm), which may weaken reception on receivers with a narrow crystal filter. Fewer bits leave more
  // room for the divider: 2 bits gives 60000.2Hz, 1 bit 59999.9Hz. The carrier is a square wave either way, since the
  // duty is 50%; only the frequency changes. The frequency the timer really makes is printed below and on the web UI.
  const uint8_t ledc_resolution = settings.carrier_resolution;
  ledc_duty_on = 1u << (ledc_resolution - 1);  // 50%
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  Log.printf("ledcAttach result= %d\n", ledcAttach(ledc_pin, settings.jjy_frequency, ledc_resolution));
  // ledcReadFreq() reports 0 while the duty is 0, so take the frequency from ledcChangeFrequency(): it sets the
  // same timer values again and returns the frequency the timer really makes.
  ledc_frequency = ledcChangeFrequency(ledc_pin, settings.jjy_frequency, ledc_resolution);
  Log.printf("ledcWrite result= %d\n", ledcWrite(ledc_pin, ledc_duty_off));  // carrier off until the time is valid
  Log.printf("pin= %u, resolution= %u bits, duty= %lu, freq= %lu\n", ledc_pin, ledc_resolution, (unsigned long)ledcRead(ledc_pin),
             (unsigned long)ledc_frequency);
#else
  ledc_frequency = ledcSetup(ledc_channel, settings.jjy_frequency, ledc_resolution);
  ledcAttachPin(ledc_pin, ledc_channel);
  ledcWrite(ledc_channel, ledc_duty_off);  // carrier off until the time is valid
  Log.printf("pin= %u, channel= %u, resolution= %u bits, freq= %lu\n", ledc_pin, ledc_channel, ledc_resolution,
             (unsigned long)ledc_frequency);
#endif

  // start Ticker for TCO
  // No need to align the Ticker to the 100ms slots: TcoGen() reads the slot from the clock on every call.
  tk.attach_ms(ticker_interval_ms, TcoGen);
}

// main task of TCO
void TcoGen()
{
  // statistics of ts_nsec
  static int    tk_count(0);
  static int    tk_max(0);
  static int    tk_min(0);
  static double tk_sum(0.0);
  static double tk_sq_sum(0.0);
  static int    tk_distribution[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  static int    tk_last_nsec(0);

  static time_t tk_last_sec(0);     // the second handled last
  static int    tk_last_slot(-1);   // the 100ms slot handled last, -1: none yet
  static int    tco_width(0);       // pulse width of this second in 100ms slots: 2 marker, 5 one, 8 zero
  static int    tco_value(0);       // TcoValue() of this second
  static bool   tco_on(false);      // the carrier is on

  if (!localtime_valid) {
    led_g = led_g_slow;  // localtime not valid yet
    tk_last_slot = -1;
    return;
  }
  led_g = led_g_off;  // localtime valid

  // Act on the first call in each 100ms slot. The slot is read from the clock every time, so the timing stays right
  // when SNTP steps the clock.
  clock_gettime(CLOCK_REALTIME, &ts);
  int  ts_100ms = ts.tv_nsec / 100000000;
  bool new_second = ts.tv_sec != tk_last_sec || tk_last_slot < 0;
  if (!new_second && ts_100ms == tk_last_slot) {
    return;
  }
  bool first_call = tk_last_slot < 0;
  int  prev_slot  = tk_last_slot;
  tk_last_sec  = ts.tv_sec;
  tk_last_slot = ts_100ms;

  if (new_second) {
    localtime_r(&ts.tv_sec, &td);  // td from the same clock reading as ts
    tco_value = TcoValue();
    tco_width = tco_value == marker ? 2 : tco_value != 0 ? 5 : 8;
  }

  // The carrier state follows from the second and the slot, not from the slot edges, so a clock step that skips
  // slots can't leave the carrier on past the end of its pulse. A pulse starts only in the first 100ms slot of a new
  // second, and goes on only while the clock moves forward within that second. So when SNTP steps the clock, the
  // pulse under way at that moment can only end early, never run long or into the next second, and a second whose
  // first slot was skipped, as at the start of transmission mid-second, is left blank: receivers treat both as one
  // lost second. (A step that lands in the first slot while a pulse is still on also leaves that second blank, so the
  // two pulses don't merge into one.)
  bool on = ts_100ms < tco_width && (new_second ? ts_100ms == 0 && !tco_on : tco_on && ts_100ms > prev_slot);
  if (on != tco_on) {
    tco_on = on;
    if (on) {
      TcOn();
    }
    else {
      TcOff();
      if      (tco_value == marker) Log.printf(" %d ", td.tm_sec);
      else if (tco_value != 0)      Log.print("1");
      else                          Log.print("0");
    }
  }
  if (new_second && td.tm_sec == 0) {
    Log.print(&td, "\n%A %B %d %Y %H:%M:%S\n");  // after switching the carrier, so the edge is not delayed
  }

  if (!first_call) {
    int tk_deviation = ts.tv_nsec - tk_last_nsec;
    if (tk_deviation < 0) {
      tk_deviation += 1000000000;  // 0xx - 9xx ms --> 10xx - 9xx ms
    }
    tk_deviation -= 100000000; // center 100 ms --> 0
    ++tk_count;

    if (tk_max < tk_deviation) tk_max = tk_deviation;
    if (tk_min > tk_deviation) tk_min = tk_deviation;
    tk_sum    += (double)tk_deviation;
    tk_sq_sum += (double)tk_deviation * (double)tk_deviation;

    if      (tk_deviation < -50000000) ++tk_distribution[0];  //     ~ -50ms
    else if (tk_deviation <  -5000000) ++tk_distribution[1];  //     ~  -5ms
    else if (tk_deviation <   -500000) ++tk_distribution[2];  //     ~  -0.5ms
    else if (tk_deviation <    -50000) ++tk_distribution[3];  //     ~  -0.05ms
    else if (tk_deviation <     50000) ++tk_distribution[4];  // -0.05 ~ 0.05ms
    else if (tk_deviation <    500000) ++tk_distribution[5];  //  0.05ms ~
    else if (tk_deviation <   5000000) ++tk_distribution[6];  //  0.5ms  ~
    else if (tk_deviation <  50000000) ++tk_distribution[7];  //  5ms    ~
    else                               ++tk_distribution[8];  // 50ms    ~
  }
  tk_last_nsec = ts.tv_nsec;

  if (new_second && td.tm_sec == 0 && tk_count != 0) {
    for (int i = 0; i < 9; ++i) {
      Log.printf("%d ", tk_distribution[i]);
    }
    double tk_average = tk_sum / (double)tk_count;
    double tk_variance = (tk_sq_sum - tk_sum * tk_sum / (double)tk_count) / (double)tk_count;
    double tk_std_deviation = sqrt(tk_variance);
    Log.printf("\nn= %d, ave= %.4f  sdv= %.4f  min= %d  max= %d\n", tk_count, tk_average, tk_std_deviation, tk_min, tk_max);

    // for the web UI
    portENTER_CRITICAL(&tco_stats_mux);
    tco_stats.count = tk_count;
    memcpy(tco_stats.distribution, tk_distribution, sizeof(tco_stats.distribution));
    tco_stats.average       = tk_average;
    tco_stats.std_deviation = tk_std_deviation;
    tco_stats.min           = tk_min;
    tco_stats.max           = tk_max;
    portEXIT_CRITICAL(&tco_stats_mux);
  }
}

void TcOn()
{
  CarrierWrite(ledc_duty_on);
  led_b = led_b_on;
}

void TcOff()
{
  CarrierWrite(ledc_duty_off);
  led_b = led_b_off;
}

void CarrierWrite(uint32_t duty)
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(ledc_pin, duty);
#else
  ledcWrite(ledc_channel, duty);
#endif
}

// TCO value
//   marker, 1:not zero, 0:zero
int TcoValue()
{
  int bcd_hour = Int3Bcd(td.tm_hour);
  int parity_bcd_hour = Parity8(bcd_hour);

  int bcd_minute = Int3Bcd(td.tm_min);
  int parity_bcd_minute = Parity8(bcd_minute);

  int year = td.tm_year + 1900;
  int bcd_year = Int3Bcd(year);

  int bcd_days = Int3Bcd(td.tm_yday + 1);  // day of the year 1..366, td.tm_yday starts from 0

  int day_of_week = td.tm_wday;

  int tco;
  switch (td.tm_sec) {
    case  0: tco = marker;              break;
    case  1: tco = bcd_minute & 0x40;   break;
    case  2: tco = bcd_minute & 0x20;   break;
    case  3: tco = bcd_minute & 0x10;   break;
    case  4: tco = 0;                   break;
    case  5: tco = bcd_minute & 0x08;   break;
    case  6: tco = bcd_minute & 0x04;   break;
    case  7: tco = bcd_minute & 0x02;   break;
    case  8: tco = bcd_minute & 0x01;   break;
    case  9: tco = marker;              break;

    case 10: tco = 0;                   break;
    case 11: tco = 0;                   break;
    case 12: tco = bcd_hour & 0x20;     break;
    case 13: tco = bcd_hour & 0x10;     break;
    case 14: tco = 0;                   break;
    case 15: tco = bcd_hour & 0x08;     break;
    case 16: tco = bcd_hour & 0x04;     break;
    case 17: tco = bcd_hour & 0x02;     break;
    case 18: tco = bcd_hour & 0x01;     break;
    case 19: tco = marker;              break;

    case 20: tco = 0;                   break;
    case 21: tco = 0;                   break;
    case 22: tco = bcd_days & 0x200;    break;
    case 23: tco = bcd_days & 0x100;    break;
    case 24: tco = 0;                   break;
    case 25: tco = bcd_days & 0x080;    break;
    case 26: tco = bcd_days & 0x040;    break;
    case 27: tco = bcd_days & 0x020;    break;
    case 28: tco = bcd_days & 0x010;    break;
    case 29: tco = marker;              break;

    case 30: tco = bcd_days & 0x008;    break;
    case 31: tco = bcd_days & 0x004;    break;
    case 32: tco = bcd_days & 0x002;    break;
    case 33: tco = bcd_days & 0x001;    break;
    case 34: tco = 0;                   break;
    case 35: tco = 0;                   break;
    case 36: tco = parity_bcd_hour;     break;
    case 37: tco = parity_bcd_minute;   break;
    case 38: tco = 0;                   break;
    case 39: tco = marker;              break;

    case 40: tco = 0;                   break;
    case 41: tco = bcd_year & 0x80;     break;
    case 42: tco = bcd_year & 0x40;     break;
    case 43: tco = bcd_year & 0x20;     break;
    case 44: tco = bcd_year & 0x10;     break;
    case 45: tco = bcd_year & 0x08;     break;
    case 46: tco = bcd_year & 0x04;     break;
    case 47: tco = bcd_year & 0x02;     break;
    case 48: tco = bcd_year & 0x01;     break;
    case 49: tco = marker;              break;

    case 50: tco = day_of_week & 0x04;  break;
    case 51: tco = day_of_week & 0x02;  break;
    case 52: tco = day_of_week & 0x01;  break;
    case 53: tco = 0;                   break;
    case 54: tco = 0;                   break;
    case 55: tco = 0;                   break;
    case 56: tco = 0;                   break;
    case 57: tco = 0;                   break;
    case 58: tco = 0;                   break;
    case 59: tco = marker;              break;
    default: tco = 0;                   break;
  }
  return tco;
}

int Int3Bcd(int a)
{
  return (a % 10) + (a / 10 % 10 * 16) + (a / 100 % 10 * 256);
}

int Parity8(int a)
{
  int pa = a;
  for (int i = 1; i < 8; ++i) {
    pa += a >> i;
  }
  return pa % 2;
}

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// web UI: the status the page shows, and the actions it asks for
const char* BoardName()
{
  switch (M5.getBoard()) {
    case m5::board_t::board_M5AtomS3Lite: return "ATOMS3 Lite";
    case m5::board_t::board_M5AtomLite  : return "ATOM Lite";
    case m5::board_t::board_M5AtomMatrix: return "ATOM Matrix";
    default                             : return "ESP32";
  }
}

const char* ResetReason()
{
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON : return "Power on";
    case ESP_RST_EXT     : return "Reset pin";
    case ESP_RST_SW      : return "Software restart";
    case ESP_RST_PANIC   : return "Crash";
    case ESP_RST_INT_WDT :
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT     : return "Watchdog";
    case ESP_RST_BROWNOUT: return "Brownout";
    default              : return "Other";
  }
}

double Finite(double a)
{
  return std::isfinite(a) ? a : 0.0;
}

void WebUiStatusJson(Print &out)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  struct tm local_tm;
  struct tm utc_tm;
  localtime_r(&tv.tv_sec, &local_tm);
  gmtime_r(&tv.tv_sec, &utc_tm);
  int days = local_tm.tm_yday - utc_tm.tm_yday;  // -1..1, or about -365/+365 across the new year
  if (days >  1) days = -1;
  if (days < -1) days = 1;
  long utc_offset = days * 86400L + (local_tm.tm_hour - utc_tm.tm_hour) * 3600L
                  + (local_tm.tm_min - utc_tm.tm_min) * 60L + (local_tm.tm_sec - utc_tm.tm_sec);
  char zone[16];
  strftime(zone, sizeof(zone), "%Z", &local_tm);

  tco_stats_t stats;
  portENTER_CRITICAL(&tco_stats_mux);
  stats = tco_stats;
  portEXIT_CRITICAL(&tco_stats_mux);

  out.printf("{\"auth\":%s,\"host\":", WebUiAuthEnabled() ? "true" : "false");
  WebUiJsonString(out, web_hostname);
  out.printf(",\"board\":\"%s\",\"uptime\":%lu,\"reset\":\"%s\",\"heap\":%lu",
             BoardName(), (unsigned long)(esp_timer_get_time() / 1000000), ResetReason(), (unsigned long)ESP.getFreeHeap());
  out.printf(",\"epoch_ms\":%lld,\"utc_offset\":%ld,\"zone\":", (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000, utc_offset);
  WebUiJsonString(out, zone);
  out.printf(",\"time_valid\":%s,\"last_sync\":%lld,\"sync_interval\":%lu",
             localtime_valid ? "true" : "false", (long long)ntp_last_sync, (unsigned long)(sntp_get_sync_interval() / 1000));
  out.print(",\"tz\":");
  WebUiJsonString(out, settings.time_zone);
  out.print(",\"ntp\":");
  WebUiJsonString(out, settings.ntp_server);
  out.printf(",\"freq\":%lu,\"freq_actual\":%lu,\"res\":%u,\"pin\":%u",
             (unsigned long)settings.jjy_frequency, (unsigned long)ledc_frequency, settings.carrier_resolution, ledc_pin);
  out.printf(",\"wifi\":{\"status\":\"%s\",\"ssid\":", WlStatus(wifi_status));
  WebUiJsonString(out, WiFi.SSID().c_str());
  out.printf(",\"rssi\":%d,\"ip\":\"%s\",\"retries\":%d,\"retry_max\":%d}",
             WiFi.RSSI(), WiFi.localIP().toString().c_str(), wifi_retry_times, wifi_retry_max_times);
  out.printf(",\"led\":{\"enabled\":%s,\"r\":%d,\"g\":%d,\"b\":%d}",
             led_enable ? "true" : "false", (int)led_r, (int)led_g, (int)led_b);
  out.printf(",\"stats\":{\"n\":%d,\"b\":[", stats.count);
  for (int i = 0; i < 9; ++i) {
    if (i != 0) {
      out.print(',');
    }
    out.print(stats.distribution[i]);
  }
  out.printf("],\"ave\":%.4f,\"sdv\":%.4f,\"min\":%d,\"max\":%d}}",
             Finite(stats.average), Finite(stats.std_deviation), stats.min, stats.max);
}

void WebUiSetLed(bool enable)
{
  led_enable = enable;
}

void WebUiSyncTime()
{
  NtpBegin(settings.time_zone, settings.ntp_server);  // restarting SNTP asks the server now
}

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// main
const unsigned int loop_period_ms(100);
      unsigned int loop_last_ms;
const int button_atom(39);    // GPIO39
const int button_atoms3(41);  // GPIO41

void setup()
{
  // Wi-Fi issue caused by the CH552 on ATOM Lite/Matrix: M5.begin() applies M5Unified's own countermeasure
  // (it drives GPIO0 HIGH), so the Rev.2 workaround of driving GPIO0 LOW is not needed here.
  // https://twitter.com/wakwak_koba/status/1553162622479974400
  // https://www.facebook.com/groups/154504605228235/posts/699719300706760/

  // M5Unified
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;  // default=0. use Log.print(). 
  M5.begin(cfg);
  delay(3000);
#if ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE
  // ATOMS3 Lite: Serial is the USB Serial/JTAG port (HWCDC). Only loop() writes to it (see BF_Log.h), so a USB host
  // that has stopped reading can't delay the time code, but it must not hold up loop() and the web UI either:
  // output that can't be queued at once is dropped instead.
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 3, 9)
  // 0, not 1: from arduino-esp32 3.3.9 on, HWCDC::write() gives up only after 20 tries that each wait the timeout,
  // so 0 gives up at once, and 1 would hold up every write for 20ms while the host is not reading.
  Serial.setTxTimeoutMs(0);
#else
  // 1ms, not 0: from arduino-esp32 2.0.15 to 3.3.8, HWCDC::write() retries once per ms until its counter reaches 0,
  // so 0 never gives up (the 2.0.14 core in M5Stack board package 2.1.4 gives up at once either way).
  Serial.setTxTimeoutMs(1);
#endif
#endif
  Log.Begin();  // this task (setup() and loop()) writes the log to Serial; the Ticker only adds to the log's buffer
  Log.println();

  // settings saved from the web UI
  WebUiLoadSettings(&settings);
  Log.printf("settings: %lu Hz, %u bits, %s, %s\n", (unsigned long)settings.jjy_frequency, settings.carrier_resolution,
             settings.time_zone, settings.ntp_server);

  // FastLED
#if defined (CONFIG_IDF_TARGET_ESP32S3)  // FastLED requires strict constant for the pin number 
  FastLED.addLeds<WS2811, led_pin_atoms3lite, GRB>(leds, led_num);
  Log.println("FastLED add LEDs for ATOMS3 Lite");
#else
  pinMode(led_pin_atom, OUTPUT);  // M5.begin() leaves GPIO27 routed to the touch sensor (RTC IO); this routes it back
  FastLED.addLeds<WS2811, led_pin_atom, GRB>(leds, led_num);
  Log.println("FastLED add LEDs for M5Atom Lite/Matrix");
#endif
  FastLED.setBrightness(led_brightness);
  for (int i = 0; i < led_num; ++i) {
    leds[i] = 0;
  }

  // No RTC: the time code waits for the first NTP reply. Until then the clock is set to a placeholder time, because
  // getLocalTime() treats years before 2016 as not set; localtime_valid keeps the carrier off meanwhile.
  Log.print("set the localtime temporarily until NTP replies\n");
  td.tm_year = 117;  // 2017 > 2016, getLocalTime() returns true
  td.tm_mon  = 0;    // January
  td.tm_mday = 1;
  td.tm_hour = 0;
  td.tm_min  = 0;
  td.tm_sec  = 0;
  struct timeval tv = { mktime(&td), 0 };
  settimeofday(&tv, NULL);
  getLocalTime(&td);
  Log.print(&td, "localtime: %A, %B %d %Y %H:%M:%S\n");
  // print sample: must be < 64
  //....:....1....:....2....:....3....:....4....:....5....:....6....
  //localtime: Wednesday, September 11 2021 11:10:46

  // WiFi start
  WiFiManager wm;  // blocking mode only

  // erase SSID/Key to force rewrite
  int button_pin = button_atom;
  if (M5.getBoard() == m5::board_t::board_M5AtomS3Lite) {
    button_pin = button_atoms3;
  }
  if (digitalRead(button_pin) == LOW) {
    wm.resetSettings();
  }

  // WiFi connect
  wm.setConfigPortalTimeout(wifi_config_portal_timeout_sec);
  wm.setAPCallback(WifiConfigModeCallback);
  wm.setHostname(web_hostname);  // the name the router shows
  wm.autoConnect();
  WiFi.setSleep(false);  // https://macsbug.wordpress.com/2021/05/02/buttona-on-m5stack-does-not-work-properly/
  wifi_retry_last_ms = millis() - wifi_retry_interval_ms;

  // NTP start
  NtpBegin(settings.time_zone, settings.ntp_server);

  // TCO start
  TcoInit();

  // web UI start
  WebUiBegin(web_hostname, web_user, web_password);

  // clear button of erase SSID/Key
  M5.update();

  // loop control
  loop_last_ms = millis();
}

void loop()
{
  M5.update();
  LedShow();

  WifiCheck();
  if (NtpSynced()) {
    localtime_valid = true;  // SNTP sync completed
    ntp_last_sync = time(NULL);
  };
  WebUiHandle();
  Log.Flush();  // what the Ticker and the SNTP callback logged

  // button: TCO monitor on/off
  if (M5.BtnA.wasReleased()) {
    led_enable = !led_enable;
  }

  // loop control
  unsigned int delay_ms(0);
  unsigned int elapse_ms = millis() - loop_last_ms;
  if (elapse_ms < loop_period_ms) {
    delay_ms = loop_period_ms - elapse_ms;
  }
  delay(delay_ms);
  loop_last_ms = millis();
//  Log.printf("loop elapse = %dms\n", elapse_ms);  // for monitoring elapsed time
}
