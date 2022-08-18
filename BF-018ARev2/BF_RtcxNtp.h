// Copyright 2021 BotanicFields, Inc.
// for RTCx(PCf8563) and NTP

#pragma once

#include <sys/time.h>  // for struct timeval
#include <sntp.h>      // for sntp_sync_status, https://github.com/espressif/arduino-esp32 >= 1.0.6

// examples
// const char* time_zone  = "JST-9";
// const char* ntp_server = "pool.ntp.org";
void NtpBegin(const char* time_zone, const char* ntp_server);
void SntpTimeSyncNotificationCallback(struct timeval *tv);
bool RtcxUpdate(bool rtcx_avail = true);
void PrintSntpStatus(const char* header, sntp_sync_status_t sntp_sync_status);
bool SetTimeFromRtcx(const char* time_zone);
