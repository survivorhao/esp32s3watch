#pragma once

#include  "wifi_connect.h"

#define RTC_MAGIC_FLAG 0xA5A5A5A5

//a variable stored in rtc ram  which indicate whether local time synchoronized or not 
extern RTC_NOINIT_ATTR uint32_t rtc_local_time_syned_flag;

void rtc_local_time_syned_flag_init(void);

void time_register_sntp_handler(void);

