// Copyright 2021 BotanicFields, Inc.
// for RTCx(PCf8563) and NTP

#include <Arduino.h>
#include "BF_Pcf8563.h"
#include "BF_RtcxNtp.h"

bool sntp_sync_status_complete(false);

void NtpBegin(const char* time_zone, const char* ntp_server)
{
  configTzTime(time_zone, ntp_server);
  Serial.printf("NtpBegin: config TZ time = %s\n", time_zone);
  Serial.printf("NtpBegin: SNTP sync mode = %d (0:IMMED 1:SMOOTH)\n", sntp_get_sync_mode());
  Serial.printf("NtpBegin: SNTP sync interval = %dms\n", sntp_get_sync_interval());
  sntp_sync_status_complete = false;
  sntp_set_time_sync_notification_cb(SntpTimeSyncNotificationCallback);
}

void SntpTimeSyncNotificationCallback(struct timeval *tv)
{
  sntp_sync_status_t sntp_sync_status = sntp_get_sync_status();
  PrintSntpStatus("SNTP callback:", sntp_sync_status);
  if (sntp_sync_status == SNTP_SYNC_STATUS_COMPLETED) {
    sntp_sync_status_complete = true;
  }
}

bool RtcxUpdate(bool rtcx_avail)
{
  if (sntp_sync_status_complete) {
    sntp_sync_status_complete = false;

    struct tm tm_sync;
    getLocalTime(&tm_sync);
    Serial.print(&tm_sync, "SNTP sync: %A, %B %d %Y %H:%M:%S\n");
    // print sample: must be < 64
    //....:....1....:....2....:....3....:....4....:....5....:....6....
    //SNTP sync: Wednesday, September 11 2021 11:10:46

    if (rtcx_avail) {
      if (rtcx.WriteTime(&tm_sync) == 0) {
        Serial.print("RTCx updated\n");
      }
      else {
        Serial.print("RTCx update failed\n");
      }
    }
    return true;
  }
  return false;
}

void PrintSntpStatus(const char* header, sntp_sync_status_t sntp_sync_status)
{
  static const char* sntp_sync_status_str[] = {
    "SNTP_SYNC_STATUS_RESET       ",  // 0
    "SNTP_SYNC_STATUS_COMPLETED   ",  // 1
    "SNTP_SYNC_STATUS_IN_PROGRESS ",  // 2
    "sntp_sync_status invalid     ",  // 3
  };
  int sntp_sync_status_index = 3;
  if (sntp_sync_status >= 0 && sntp_sync_status <= 2) {
    sntp_sync_status_index = sntp_sync_status;
  }
  Serial.printf("%s status = %d %s\n", header, sntp_sync_status, sntp_sync_status_str[sntp_sync_status_index]);
}

bool SetTimeFromRtcx(const char* time_zone)
{
  bool rtcx_valid(false);
  struct tm tm_init;

  setenv("TZ", time_zone, 1);
  tzset();  // assign timezone with setenv for mktime()

  if (rtcx.ReadTime(&tm_init) == 0) {
    rtcx_valid = true;
    struct timeval tv = { mktime(&tm_init), 0 };
    settimeofday(&tv, NULL);
    Serial.print("RTCx valid, the localtime was set\n");
  } else {
    Serial.print("RTCx not valid\n");
  }
  return rtcx_valid;
}
