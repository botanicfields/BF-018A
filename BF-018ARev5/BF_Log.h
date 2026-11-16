// BF-018A Rev.5
// serial output with a copy kept for the web UI
//
#pragma once
#include <Arduino.h>

// Keeps the latest output in a ring buffer that the web UI reads, and copies it to Serial.
// Safe to use from any task: only the task that called Begin() (the one running setup() and loop()) ever writes to
// Serial. Output from other tasks, such as the Ticker that makes the time code or the SNTP callback, only goes into
// the ring buffer and reaches Serial at the next Flush(), so a slow or stalled serial port can't delay the time code.
class LogTee : public Print {
 public:
  static const size_t m_size = 4096;  // bytes kept

  void   Begin();  // call from setup(): this task writes to Serial from now on
  void   Flush();  // copy what other tasks wrote to Serial; call from loop()
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buffer, size_t size) override;

  // Copy the kept output from position `from` (bytes written since boot) into `buffer`.
  // If `from` is no longer kept, or is ahead (e.g. the page saw an earlier boot), copying starts at the oldest byte kept.
  // `first` receives the position actually copied from. Returns the number of bytes copied.
  size_t Read(uint32_t from, char *buffer, size_t buffer_size, uint32_t *first);

 private:
  char         m_buffer[m_size];
  uint32_t     m_total = 0;  // bytes written since boot
  uint32_t     m_sent  = 0;  // bytes copied to Serial (or skipped because the ring overwrote them first)
  TaskHandle_t m_serial_task = nullptr;
};

extern LogTee Log;
