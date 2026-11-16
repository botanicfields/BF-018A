// Host test of the Rev.5 time code: TcoValue() and TcoGen() are compiled from BF-018ARev5.ino itself
// (run_host_tests.py extracts them into tco.inc) with stubs for the ESP32 parts.
//  1. TcoValue() against an encoder written separately from the JJY frame layout, for every second of 2024-2031.
//  2. TcoGen() driven by a simulated 10ms Ticker: pulse timing in normal running, and when SNTP steps the clock.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

#define ESP_ARDUINO_VERSION_MAJOR 3
struct LogStub {
  void printf(const char *, ...) {}
  void print(const char *) {}
  void print(struct tm *, const char *) {}
} Log;
typedef int portMUX_TYPE;
#define portENTER_CRITICAL(x)
#define portEXIT_CRITICAL(x)
enum led_g_t { led_g_off, led_g_slow };
enum led_b_t { led_b_off, led_b_on };
led_g_t led_g;
led_b_t led_b;
volatile bool localtime_valid = true;
struct tm td;
struct timespec ts;
const int marker(0xff);
uint8_t  ledc_pin = 5;
uint32_t ledc_duty_on = 128, ledc_duty_off = 0;
struct tco_stats_t { int count; int distribution[9]; double average, std_deviation; int min, max; } tco_stats;
portMUX_TYPE tco_stats_mux;

// simulated clocks: real time in us; the device clock is real time + offset_us
static int64_t real_us = 0, offset_us = 0;
extern "C" int clock_gettime(clockid_t, struct timespec *t)
{
  int64_t d = real_us + offset_us;
  t->tv_sec = d / 1000000;
  t->tv_nsec = (d % 1000000) * 1000;
  return 0;
}
struct Edge { int64_t t; bool on; };
static std::vector<Edge> edges;
static bool carrier = false;
void ledcWrite(uint8_t, uint32_t duty)
{
  bool on = duty != 0;
  if (on != carrier) {
    carrier = on;
    edges.push_back({real_us, on});
  }
}

void TcoGen(); void TcOn(); void TcOff(); void CarrierWrite(uint32_t); int TcoValue(); int Int3Bcd(int); int Parity8(int);
#include "tco.inc"

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); } } while (0)

// reference encoder: 2 marker, 1, 0
static int Reference(const struct tm &t, int s)
{
  static const int markers[] = {0, 9, 19, 29, 39, 49, 59};
  for (int m : markers) if (s == m) return 2;
  int b[60] = {};
  auto put = [&](int value, const int *weights, int n, int first) {
    for (int i = 0; i < n; ++i) if (weights[i] && value >= weights[i]) { b[first + i] = 1; value -= weights[i]; }
    return value;
  };
  static const int w_min[] = {40, 20, 10, 0, 8, 4, 2, 1};
  static const int w_hour[] = {20, 10, 0, 8, 4, 2, 1};
  static const int w_doy1[] = {200, 100, 0, 80, 40, 20, 10};
  static const int w_doy2[] = {8, 4, 2, 1};
  static const int w_year[] = {80, 40, 20, 10, 8, 4, 2, 1};
  put(t.tm_min, w_min, 8, 1);
  put(t.tm_hour, w_hour, 7, 12);
  put(put(t.tm_yday + 1, w_doy1, 7, 22), w_doy2, 4, 30);
  for (int i = 12; i <= 18; ++i) b[36] ^= b[i];  // PA1: hour
  for (int i = 1; i <= 8; ++i) b[37] ^= b[i];    // PA2: minute
  put((t.tm_year + 1900) % 100, w_year, 8, 41);
  b[50] = (t.tm_wday >> 2) & 1;
  b[51] = (t.tm_wday >> 1) & 1;
  b[52] = t.tm_wday & 1;
  return b[s];
}

static void TestEncoder()
{
  long seconds = 0, mismatches = 0;
  for (time_t t = 1704034800; t < 1956495600; t += 60) {  // 2024-01-01 to 2032-01-01 JST: leap years, day 366
    struct tm base;
    localtime_r(&t, &base);
    for (int s = 0; s < 60; ++s) {
      td = base;
      td.tm_sec = s;
      int v = TcoValue();
      int got = v == marker ? 2 : v != 0 ? 1 : 0;
      if (got != Reference(base, s) && mismatches++ < 5) {
        std::printf("FAIL: TcoValue() at %lld second %d: %d, expected %d\n", (long long)t, s, got, Reference(base, s));
      }
      ++seconds;
    }
  }
  failures += mismatches != 0;
  std::printf("encoder: %ld seconds checked, %ld mismatches\n", seconds, mismatches);
}

// Run the Ticker for `secs` seconds of real time; step the device clock by step_us at real time step_at_us.
// Returns the number of pulses that are not what the second they start in calls for.
static int Run(int64_t start_dev_us, double secs, int64_t step_at_us, int64_t step_us, double *longest_ms)
{
  edges.clear();
  carrier = false;
  offset_us = start_dev_us;
  bool stepped = false;
  for (real_us = 0; real_us < (int64_t)(secs * 1e6); real_us += 10000 + (std::rand() % 400 - 200)) {  // 10ms +-0.2ms
    if (!stepped && step_at_us >= 0 && real_us >= step_at_us) {
      offset_us += step_us;
      stepped = true;
    }
    TcoGen();
  }
  int off_spec = 0;
  *longest_ms = 0;
  for (size_t i = 0; i + 1 < edges.size(); i += 2) {
    CHECK(edges[i].on && !edges[i + 1].on, "carrier edges out of order");
    double width_ms = (edges[i + 1].t - edges[i].t) / 1e3;
    int64_t dev = edges[i].t + offset_us - (stepped && edges[i].t < step_at_us ? step_us : 0);
    double phase_ms = (dev % 1000000) / 1e3;
    time_t sec = dev / 1000000;
    localtime_r(&sec, &td);
    int v = TcoValue();
    double want_ms = v == marker ? 200 : v != 0 ? 500 : 800;
    if (!(phase_ms < 11 && std::fabs(width_ms - (want_ms - phase_ms)) < 11)) {
      ++off_spec;
    }
    if (width_ms - want_ms > *longest_ms) {
      *longest_ms = width_ms - want_ms;
    }
  }
  return off_spec;
}

static void TestTicker()
{
  const int64_t t0 = 1794816000LL * 1000000;  // 2026-11-16 00:00 JST
  double longest_ms;
  int off = Run(t0 + 437000, 180, -1, 0, &longest_ms);  // starts mid-second
  CHECK(off == 0, "no clock step: %d pulses off spec", off);
  CHECK(edges.size() >= 2 * 179, "no clock step: only %zu edges in 3 minutes", edges.size());
  std::printf("ticker: 3 minutes without a step, %zu pulses, %d off spec\n", edges.size() / 2, off);

  // A step may spoil the pulse under way, but no other, and may only shorten it (by less than a 100ms slot otherwise).
  const int64_t steps_ms[] = {30, -30, 150, -150, 350, -350, 650, -650, 2400, -1700, 60000, -60000};
  int cases = 0;
  for (int64_t step_ms : steps_ms) {
    for (int at_ms = 1000; at_ms < 2000; at_ms += 10) {
      off = Run(t0, 4, at_ms * 1000LL, step_ms * 1000, &longest_ms);
      CHECK(off <= 1, "step %+lld ms at %d ms: %d pulses off spec", (long long)step_ms, at_ms, off);
      CHECK(longest_ms < 100, "step %+lld ms at %d ms: a pulse %.1f ms longer than its value", (long long)step_ms, at_ms, longest_ms);
      ++cases;
    }
  }
  std::printf("ticker: %d clock steps checked\n", cases);
}

int main()
{
  setenv("TZ", "JST-9", 1);
  tzset();
  std::srand(1);
  TestEncoder();
  TestTicker();
  std::printf(failures ? "tco_test: FAILED\n" : "tco_test: passed\n");
  return failures != 0;
}
