// Copyright 2021 BotanicFields, Inc.
// for NTP (Rev.5: the RTC support of Rev.3/Rev.4 is not included)
//
#include <Arduino.h>
#include "BF_Log.h"
#include "BF_Ntp.h"

// set by the SNTP callback, which runs in the lwIP task; read and cleared by NtpSynced() in loop()
static volatile bool sntp_sync_status_complete(false);

static void PrintSntpStatus(const char* header, sntp_sync_status_t sntp_sync_status)
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
  Log.printf("%s status = %d %s\n", header, sntp_sync_status, sntp_sync_status_str[sntp_sync_status_index]);
}

static void SntpTimeSyncNotificationCallback(struct timeval *tv)
{
  sntp_sync_status_t sntp_sync_status = sntp_get_sync_status();
  PrintSntpStatus("SNTP callback:", sntp_sync_status);
  if (sntp_sync_status == SNTP_SYNC_STATUS_COMPLETED) {
    sntp_sync_status_complete = true;
  }
}

void NtpBegin(const char* time_zone, const char* ntp_server)
{
  configTzTime(time_zone, ntp_server);
  Log.printf("NtpBegin: config TZ time = %s\n", time_zone);
  Log.printf("NtpBegin: SNTP sync mode = %d (0:IMMED 1:SMOOTH)\n", sntp_get_sync_mode());
  Log.printf("NtpBegin: SNTP sync interval = %lums\n", (unsigned long)sntp_get_sync_interval());
  sntp_sync_status_complete = false;
  sntp_set_time_sync_notification_cb(SntpTimeSyncNotificationCallback);
}

bool NtpSynced()
{
  if (!sntp_sync_status_complete) {
    return false;
  }
  sntp_sync_status_complete = false;

  struct tm tm_sync;
  getLocalTime(&tm_sync);
  Log.print(&tm_sync, "SNTP sync: %A, %B %d %Y %H:%M:%S\n");
  // print sample: must be < 64
  //....:....1....:....2....:....3....:....4....:....5....:....6....
  //SNTP sync: Wednesday, September 11 2021 11:10:46
  return true;
}
