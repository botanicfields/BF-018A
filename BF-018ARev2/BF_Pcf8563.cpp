// Copyright 2021 BotanicFields, Inc.
// PCF8563 RTC for M5 Series

#include <Arduino.h>
#include "BF_Pcf8563.h"

int Pcf8563::Begin(TwoWire &wire)
{
  m_wire = &wire;

  // clear registers
  external_clock_test_mode = false;
  source_clock_stoped      = false;
  power_on_reset_override  = true;
  if (WriteControl() != 0) return 1;

  clock_out_active =false;
  clock_out        = fco_32768Hz;
  if (WriteClockOut() != 0) return 1;

  alarm_minute_enable  = false;  alarm_minute  = 0;
  alarm_hour_enable    = false;  alarm_hour    = 0;
  alarm_day_enable     = false;  alarm_day     = 0;
  alarm_weekday_enable = false;  alarm_weekday = 0;
  if (WriteAlarm() != 0) return 1;

  timer_enable = false;
  timer_source = fts_4096Hz;
  timer        = 0;
  if (WriteTimer() != 0) return 1;

  timer_interrupt_pulse_mode = false;
  alarm_flag_active          = false;
  timer_flag_active          = false;
  alarm_interrupt_enable     = false;
  timer_interrupt_enable     = false;
  if (WriteInterrupt() != 0) return 1;

  return 0;
}

int Pcf8563::ReadTime(struct tm *tm_now)
{
  if (ReadReg(0x02, 7) != 7) return 1;  // ReadReg error
  if (m_reg[0x02] & 0x80) return 2;     // invalid time
  tm_now->tm_sec  = Bcd2Int(m_reg[0x02] & 0x7f);
  tm_now->tm_min  = Bcd2Int(m_reg[0x03] & 0x7f);
  tm_now->tm_hour = Bcd2Int(m_reg[0x04] & 0x3f);
  tm_now->tm_mday = Bcd2Int(m_reg[0x05] & 0x3f);
  tm_now->tm_wday = Bcd2Int(m_reg[0x06] & 0x07);      // 0:Sun, 1:Mon, .. 6:Sat
  tm_now->tm_mon  = Bcd2Int(m_reg[0x07] & 0x1f) - 1;  // tm month: 0..11
  tm_now->tm_year = Bcd2Int(m_reg[0x08] & 0xff);
  if ((m_reg[0x07] & 0x80) != 0)
    tm_now->tm_year += 100;  // century bit
  return 0;
}

int Pcf8563::WriteTime(struct tm *tm_now)
{
  m_reg[0x02] = Int2Bcd(tm_now->tm_sec);
  m_reg[0x03] = Int2Bcd(tm_now->tm_min);
  m_reg[0x04] = Int2Bcd(tm_now->tm_hour);
  m_reg[0x05] = Int2Bcd(tm_now->tm_mday);
  m_reg[0x06] = Int2Bcd(tm_now->tm_wday);     // 0:Sun, 1:Mon, .. 6:Sat
  m_reg[0x07] = Int2Bcd(tm_now->tm_mon + 1);  // rtc month: 1..12
  m_reg[0x08] = Int2Bcd(tm_now->tm_year % 100);
  if (tm_now->tm_year >= 100)
    m_reg[0x07] |= 0x80;  // century bit
  return WriteReg(0x02, 7);
}

// to enable alarm, give valid value for arguments of minute, hour, day, weekday
int Pcf8563::SetAlarm(int minute, int hour, int day, int weekday)
{
  alarm_minute_enable  = false;
  alarm_hour_enable    = false;
  alarm_day_enable     = false;
  alarm_weekday_enable = false;
  if (minute  >= 0 && minute  <= 59) { alarm_minute_enable  = true;  alarm_minute  = minute;   }
  if (hour    >= 0 && hour    <= 23) { alarm_hour_enable    = true;  alarm_hour    = hour;     }
  if (day     >= 1 && day     <= 31) { alarm_day_enable     = true;  alarm_day     = day;      }
  if (weekday >= 0 && weekday <=  6) { alarm_weekday_enable = true;  alarm_weekday = weekday;  }
  return WriteAlarm();
}

int Pcf8563::DisableAlarm()
{
  return SetAlarm();
}

// interrupt_en = true: enable, false: disable
int Pcf8563::EnableAlarmInterrupt(bool enable_interrupt, bool flag_keep)
{
  if (ReadInterrupt() != 0) return 1;
  alarm_flag_active      = flag_keep;
  alarm_interrupt_enable = enable_interrupt;
  return WriteInterrupt();
}

int Pcf8563::DisableAlarmInterrupt()
{
  return EnableAlarmInterrupt(false);  // disable and clear
}

// Set timer_source and timer to the argument timer_sec as close as possible
//   returns actual timer value realized by timer_source and timer,
//   return 0.0: timer_sec is out of range, the timer was not set
//   return <>0.0: The timer is enabled.
double Pcf8563::SetTimer(double timer_sec)
{
  if (timer_sec < 1.0 / 4096.0) return 0.0;  // minimum 0.00024414s
  if (timer_sec > 255.0 * 60.0) return 0.0;  // maximum 15,300s

  if (timer_sec >= 240.0) {            // cross over at 240s not 255s
    timer_source = fts_1_60th_Hz;
    timer = timer_sec / 60.0 + 0.5;    // +0.5 for rounding
  }
  else if (timer_sec > 255.0 / 64.0) {
    timer_source = fts_1Hz;
    timer = timer_sec + 0.5;           // +0.5 for rounding
  }
  else if (timer_sec > 255.0 / 4096.0) {
    timer_source = fts_64Hz;
    timer = timer_sec * 64.0 + 0.5;    // +0.5 for rounding
  }
  else {
    timer_source = fts_4096Hz;
    timer = timer_sec * 4096.0 + 0.5;  // +0.5 for rounding
  }
  timer_enable = true;
  if (WriteTimer() != 0) return 0.0;

  switch (timer_source) {
    case 0: return timer / 4096.0;  break;
    case 1: return timer / 64.0;    break;
    case 2: return timer / 1.0;     break;
    case 3: return timer * 60.0;    break;
    default: return 0.0;  break;
  }
}

// timer_en = true: enable, false: disable
int Pcf8563::EnableTimer(bool enable_timer)
{
  if (ReadReg(0x0e, 1) != 1) return 1;
  switch (m_reg[0x0e] & 0x03) {
    case 0: timer_source = fts_4096Hz;     break;
    case 1: timer_source = fts_64Hz;       break;
    case 2: timer_source = fts_1Hz;        break;
    case 3: timer_source = fts_1_60th_Hz;  break;
    default: break;
  }
  timer_enable = enable_timer;
  m_reg[0x0e] = timer_source;
  if (timer_enable)
    m_reg[0x0e] |= 0x80;
  return WriteReg(0x0e, 1);
}

int Pcf8563::DisableTimer()
{
  return EnableTimer(false);
}

// interrupt_enable = true: enable, false: disable
int Pcf8563::EnableTimerInterrupt(bool enable_interrupt, bool pulse_mode, bool keep_flag)
{
  if (ReadInterrupt() != 0) return 1;
  timer_interrupt_pulse_mode = pulse_mode;
  timer_flag_active          = keep_flag;
  timer_interrupt_enable     = enable_interrupt;
  if (WriteInterrupt() != 0) return 1;
  return 0;
}

int Pcf8563::DisableTimerInterrupt()
{
  return EnableTimerInterrupt(false);  // disable and clear
}

int Pcf8563::GetInterrupt()
{
  if (ReadInterrupt() != 0) return 4;
  int flag_got(0);
  if (alarm_flag_active) { flag_got |= 0x02;  alarm_flag_active = false; }
  if (timer_flag_active) { flag_got |= 0x01;  timer_flag_active = false; }
  if (flag_got != 0)
    if (WriteInterrupt() != 0) return 4;
  return flag_got;
}

int Pcf8563::ClockOutForTrimmer(bool enable_clko)  // clock out 1Hz
{
  if (enable_clko) {
    // CLKO(clock out) to adjust trimmer
    clock_out        = fco_1Hz;
    clock_out_active = true;
    if (WriteClockOut() != 0) return 1;
  }
  else {
    clock_out_active = false;
    if (WriteClockOut() != 0) return 1;
  }
  return 0;
}

Pcf8563::Pcf8563()
{
  m_reg = new uint8_t[m_reg_size];
  for (int i = 0; i < m_reg_size; ++i)
    m_reg[i] = 0;
}

Pcf8563::~Pcf8563()
{
  delete [] m_reg;
}

int Pcf8563::ReadControl()
{
  if (ReadReg(0x00, 1) != 1) return 1;
  external_clock_test_mode = m_reg[0x00] & 0x80;
  source_clock_stoped      = m_reg[0x00] & 0x20;
  power_on_reset_override  = m_reg[0x00] & 0x08;
  return 0;
}

int Pcf8563::WriteControl()
{
  m_reg[0x00] = 0x00;
  if (external_clock_test_mode) m_reg[0x00] |= 0x80;
  if (source_clock_stoped     ) m_reg[0x00] |= 0x20;
  if (power_on_reset_override ) m_reg[0x00] |= 0x08;
  return WriteReg(0x00, 1);
}

int Pcf8563::ReadInterrupt()
{
  if (ReadReg(0x01, 1) != 1) return 1;
  timer_interrupt_pulse_mode = m_reg[0x01] & 0x10;
  alarm_flag_active          = m_reg[0x01] & 0x08;
  timer_flag_active          = m_reg[0x01] & 0x04;
  alarm_interrupt_enable     = m_reg[0x01] & 0x02;
  timer_interrupt_enable     = m_reg[0x01] & 0x01;
  return 0;
}

int Pcf8563::WriteInterrupt()
{
  m_reg[0x01] = 0x00;
  if (timer_interrupt_pulse_mode) m_reg[0x01] |= 0x10;
  if (alarm_flag_active         ) m_reg[0x01] |= 0x08;
  if (timer_flag_active         ) m_reg[0x01] |= 0x04;
  if (alarm_interrupt_enable    ) m_reg[0x01] |= 0x02;
  if (timer_interrupt_enable    ) m_reg[0x01] |= 0x01;
  return WriteReg(0x01, 1);
}

int Pcf8563::ReadAlarm()
{
  if (ReadReg(0x09, 4) != 4) return 1;
  alarm_minute_enable  = (m_reg[0x09] & 0x80) == 0;
  alarm_hour_enable    = (m_reg[0x0a] & 0x80) == 0;
  alarm_day_enable     = (m_reg[0x0b] & 0x80) == 0;
  alarm_weekday_enable = (m_reg[0x0c] & 0x80) == 0;
  alarm_minute  = Bcd2Int(m_reg[0x09] & 0x7f);
  alarm_hour    = Bcd2Int(m_reg[0x0a] & 0x3f);
  alarm_day     = Bcd2Int(m_reg[0x0b] & 0x3f);
  alarm_weekday = Bcd2Int(m_reg[0x0c] & 0x07);
  return 0;
}

int Pcf8563::WriteAlarm()
{
  m_reg[0x09] = Int2Bcd(alarm_minute );
  m_reg[0x0a] = Int2Bcd(alarm_hour   );
  m_reg[0x0b] = Int2Bcd(alarm_day    );
  m_reg[0x0c] = Int2Bcd(alarm_weekday);
  if (!alarm_minute_enable ) m_reg[0x09] |= 0x80;
  if (!alarm_hour_enable   ) m_reg[0x0a] |= 0x80;
  if (!alarm_day_enable    ) m_reg[0x0b] |= 0x80;
  if (!alarm_weekday_enable) m_reg[0x0c] |= 0x80;
  return WriteReg(0x09, 4);
}

int Pcf8563::ReadClockOut()
{
  if (ReadReg(0x0d, 1) != 1) return 1;
  clock_out_active = m_reg[0x0d] & 0x80;
  switch (m_reg[0x0d] & 0x03) {
    case 0: clock_out = fco_32768Hz;  break;
    case 1: clock_out = fco_1024Hz;   break;
    case 2: clock_out = fco_32Hz;     break;
    case 3: clock_out = fco_1Hz;      break;
    default: break;
  }
  return 0;
}

int Pcf8563::WriteClockOut()
{
  m_reg[0x0d] = clock_out;
  if (clock_out_active)
    m_reg[0x0d] |= 0x80;
  return WriteReg(0x0d, 1);
}

int Pcf8563::ReadTimer()
{
  if (ReadReg(0x0e, 2) != 2) return 1;
  timer_enable = m_reg[0x0e] & 0x80;
  switch (m_reg[0x0e] & 0x03) {
    case 0: timer_source = fts_4096Hz;     break;
    case 1: timer_source = fts_64Hz;       break;
    case 2: timer_source = fts_1Hz;        break;
    case 3: timer_source = fts_1_60th_Hz;  break;
    default: break;
  }
  timer = m_reg[0x0f];
  return 0;
}

int Pcf8563::WriteTimer()
{
  m_reg[0x0e] = timer_source;
  if (timer_enable)
    m_reg[0x0e] |= 0x80;
  m_reg[0x0f] = timer;
  return WriteReg(0x0e, 2);
}

int Pcf8563::Int2Bcd(int int_num)
{
  return int_num / 10 * 16 + int_num % 10;
}

int Pcf8563::Bcd2Int(int bcd_num)
{
  return bcd_num / 16 * 10 + bcd_num % 16;
}

size_t Pcf8563::ReadReg(int reg_start, size_t read_length)
{
  const bool send_stop(true);

  m_wire->beginTransmission(m_i2c_address);
  m_wire->write(reg_start);
  if (m_wire->endTransmission(!send_stop) != 0) {
    Serial.print("[PCF8563]ERROR ReadReg write\n");
    return 0;  // command error
  }
  m_wire->requestFrom(m_i2c_address, read_length);
  size_t i = 0;
  while (m_wire->available())
    m_reg[reg_start + i++] = m_wire->read();
  if (i != read_length)
    Serial.print("[PCF8563]ERROR ReadReg read\n");
  return i;
}

int Pcf8563::WriteReg(int reg_start, size_t write_length)
{
  m_wire->beginTransmission(m_i2c_address);
  m_wire->write(reg_start);
  for (int i = 0; i < write_length; ++i)
    m_wire->write(m_reg[reg_start + i]);
  int return_code = m_wire->endTransmission();
  if (return_code != 0)
    Serial.print("[PCF8563]ERROR WriteReg\n");
  return return_code;
}

// the instance "rtc external"
Pcf8563 rtcx;
