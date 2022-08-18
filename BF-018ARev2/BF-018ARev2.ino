// copyright 2022 BotanicFields, Inc.
// BF-018A Rev.2
// JJY Simulator for M5Atom / M5Atom Lite

#include <M5Atom.h>
#include <Ticker.h>
#include <WiFi.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include "BF_Pcf8563.h"
#include "BF_RtcxNtp.h"

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// for NTP
const char* time_zone  = "JST-9";
const char* ntp_server = "pool.ntp.org";
bool localtime_valid(false);
bool rtcx_avail(false);

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// for LED
const int led_on_r(0x80);  // PWM 0..0xFF
const int led_on_g(0x80);  // PWM 0..0xFF
const int led_on_b(0x80);  // PWM 0..0xFF
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
void LedShow();

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// for WiFi
const int wifi_config_portal_timeout_sec(60);
const unsigned int wifi_retry_interval_ms(60000);
      unsigned int wifi_retry_last_ms(0);
const int wifi_retry_max_times(3);
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
    Serial.printf("[WiFi]%s\n", WlStatus(wifi_status));
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
    Serial.print("[WiFi]connect failed: rebooting..\n");
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
    Serial.print("[WiFi]disconnect timeout: rebooting..\n");
    ESP.restart();
    return;
  }

  // reconnect, and reboot if reconnection fails
  Serial.printf("[WiFi]reconnect %d\n", wifi_retry_times);
  if (!WiFi.reconnect()) {
    Serial.print("[WiFi]reconnect failed: rebooting..\n");
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
const int ticker_interval_ms(100);  // 100ms
const int marker(0xff);  // marker code TcoValue() returns

// PWM for TCO signal
const uint8_t  ledc_pin(22);           // GPIO22 for TCO
const uint8_t  ledc_channel(0);
const uint32_t ledc_frequency(60000);  // 40kHz(east), 60kHz(west)
const uint8_t  ledc_resolution(2);     // 2^2 = 4
const uint32_t ledc_duty_on(2);        // 2/4 = 50%
const uint32_t ledc_duty_off(0);       // 0

// real time
struct tm       td;  // time of day: year, month, day, day of week, hour, minute, second
struct timespec ts;  // time spec: second, nano-second

void TcoInit()
{
  // carrier for TCO
  uint32_t ledc_freq_get = ledcSetup(ledc_channel, ledc_frequency, ledc_resolution);
  Serial.printf("ledc frequency get = %d\n", ledc_freq_get);
  ledcAttachPin(ledc_pin, ledc_channel);

  // wait until middle of 100ms timing. ex. 50ms, 150ms, 250ms,..
  clock_gettime(CLOCK_REALTIME, &ts);
  delayMicroseconds((150000000 - ts.tv_nsec % 100000000) / 1000);

  // for the first sample of statistics
  clock_gettime(CLOCK_REALTIME, &ts);
  Serial.printf("ts.tv_nsec = %d\n", ts.tv_nsec);

  // start Ticker for TCO
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

  if (!localtime_valid) {
    led_g = led_g_slow;  // localtime not valid yet
    return;
  }
  led_g = led_g_off;  // localtime valid

  getLocalTime(&td);
  clock_gettime(CLOCK_REALTIME, &ts);
  int ts_100ms = ts.tv_nsec / 100000000;
  switch (ts_100ms) {
    case 0: Tco000ms();  break;
    case 2: Tco200ms();  break;
    case 5: Tco500ms();  break;
    case 8: Tco800ms();  break;
    default:  break;
  }

  if (tk_count++ != 0) {
    int tk_deviation = ts.tv_nsec - tk_last_nsec;
    if (tk_deviation < 0) {
      tk_deviation += 1000000000;  // 0xx - 9xx ms --> 10xx - 9xx ms
    }
    tk_deviation -= 100000000; // center 100 ms --> 0

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

  if ((td.tm_sec == 0) && (ts.tv_nsec < 100000000)) {
    for (int i = 0; i < 9; ++i) {
      Serial.printf("%d ", tk_distribution[i]);
    }
    double tk_average = tk_sum / (double)tk_count;
    double tk_variance = (tk_sq_sum - tk_sum * tk_sum / (double)tk_count) / (double)tk_count;
    double tk_std_deviation = sqrt(tk_variance);
    Serial.printf("\nn= %d, ave= %.4f  sdv= %.4f  min= %d  max= %d\n", tk_count, tk_average, tk_std_deviation, tk_min, tk_max);
  }
}

// TCO task at every 0ms
void Tco000ms()
{
  TcOn();
  if (td.tm_sec == 0) {
    Serial.print(&td, "\n%A %B %d %Y %H:%M:%S\n");
  }
}

// TCO task at every 200ms
void Tco200ms()
{
  if (TcoValue() == marker) {
    TcOff();
    Serial.printf(" %d ", td.tm_sec);
  }
}

// TCO task at every 500ms
void Tco500ms()
{
  if (TcoValue() != 0) {
    TcOff();
    if(TcoValue() != marker) {
      Serial.print("1");
    }
  }
}

// TCO task at every 800ms
void Tco800ms()
{
  TcOff();
  if (TcoValue() == 0) {
    Serial.print("0");
  }
}

void TcOn()
{
  ledcWrite(ledc_channel, ledc_duty_on);
  led_b = led_b_on;
}

void TcOff()
{
  ledcWrite(ledc_channel, ledc_duty_off);
  led_b = led_b_off;
}

// TCO value
//   marker, 1:not zero, 0:zero
int TcoValue()
{
  const int days_of_month[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

  int bcd_hour = Int3Bcd(td.tm_hour);
  int parity_bcd_hour = Parity8(bcd_hour);

  int bcd_minute = Int3Bcd(td.tm_min);
  int parity_bcd_minute = Parity8(bcd_minute);

  int year = td.tm_year + 1900;
  int bcd_year = Int3Bcd(year);

  int days = td.tm_mday;
  for (int i = 0; i < td.tm_mon; ++i) {  // td.tm_mon starts from 0
    days += days_of_month[i];
  }
  if ((td.tm_mon >= 2) && (((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0))) {
    ++days;
  }
  int bcd_days = Int3Bcd(days);

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
// LED
void LedShow()
{
  if (!led_enable && led_r == led_r_off && led_g == led_g_off) {
    M5.dis.drawpix(0, 0);
    return;
  }

  // LED on
  CRGB led(0);
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
  M5.dis.drawpix(0, led);
}

bool LedBlink(unsigned int period_ms)
{
  return millis() / period_ms % 2 != 0;
}

//..:....1....:....2....:....3....:....4....:....5....:....6....:....7..
// main
const unsigned int loop_period_ms(100);
      unsigned int loop_last_ms;

void setup()
{
  const bool serial_enable(true);
  const bool i2c_enable(true);  // Wire: SDA=25, SCL=21, Frequency=100kHz
  const bool display_enable(true);
  M5.begin(serial_enable, !i2c_enable, display_enable);  // Wire is not used

  // assign Wire1 to GROVE port of M5Atom to connect RTC
  const int scl1(32);            // GPIO32
  const int sda1(26);            // GPIO26
  const uint32_t freq1(100000);  // 100kHz
  Wire1.begin(sda1, scl1, freq1);
  if (rtcx.Begin(Wire1) == 0) {
    rtcx_avail = true;
    if (SetTimeFromRtcx(time_zone)) {
      localtime_valid = true;
    }
  }
  if (!localtime_valid) {
    Serial.print("RTC not valid: set the localtime temporarily\n");
    td.tm_year = 117;  // 2017 > 2016, getLocalTime() returns true
    td.tm_mon  = 0;    // January
    td.tm_mday = 1;
    td.tm_hour = 0;
    td.tm_min  = 0;
    td.tm_sec  = 0;
    struct timeval tv = { mktime(&td), 0 };
    settimeofday(&tv, NULL);
  }
  getLocalTime(&td);
  Serial.print(&td, "localtime: %A, %B %d %Y %H:%M:%S\n");
  // print sample: must be < 64
  //....:....1....:....2....:....3....:....4....:....5....:....6....
  //localtime: Wednesday, September 11 2021 11:10:46

  // WiFi start
  WiFiManager wm;  // blocking mode only

  // erase SSID/Key to force rewrite
  const int button_pin(39);  // the button on M5Atom
  if (digitalRead(button_pin) == LOW) {
    wm.resetSettings();
  }

  // WiFi connect
  wm.setConfigPortalTimeout(wifi_config_portal_timeout_sec);
  wm.setAPCallback(WifiConfigModeCallback);
  wm.autoConnect();
  WiFi.setSleep(false);  // https://macsbug.wordpress.com/2021/05/02/buttona-on-m5stack-does-not-work-properly/
  wifi_retry_last_ms = millis() - wifi_retry_interval_ms;

  // NTP start
  NtpBegin(time_zone, ntp_server);

  // TCO start
  TcoInit();

  // clear button of erase SSID/Key
  M5.update();

  // loop control
  loop_last_ms = millis();
}

void loop()
{
  M5.update();
  WifiCheck();
  if (RtcxUpdate(rtcx_avail)) {
    localtime_valid = true;  // SNTP sync completed
  };
  LedShow();

  // button: TCO monitor on/off
  if (M5.Btn.wasReleased()) {
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
//  Serial.printf("loop elapse = %dms\n", elapse_ms);  // for monitoring elapsed time
}
