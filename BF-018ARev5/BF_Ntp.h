// Copyright 2021 BotanicFields, Inc.
// for NTP (Rev.5: the RTC support of Rev.3/Rev.4 is not included)
//
#pragma once
#include <sys/time.h>  // for struct timeval
#include <esp_sntp.h>  // for sntp_sync_status

// examples
// const char* time_zone  = "JST-9";
// const char* ntp_server = "pool.ntp.org";
void NtpBegin(const char* time_zone, const char* ntp_server);  // (re)start SNTP: it asks the server at once
bool NtpSynced();  // true once after each completed SNTP sync; call from loop()
