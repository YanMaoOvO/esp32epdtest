#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>

struct LunarDate {
  int year;   // 农历年（如 2026）
  int month;  // 1..12
  int day;    // 1..30
  bool leap;  // 是否闰月
};

// 公历 -> 农历（数据范围 1900-2100）
LunarDate solarToLunar(int year, int month, int day);

const char *lunarMonthName(int month);
const char *lunarDayName(int day);
const char *weekdayName(int weekday);  // 0 = 星期日
const char *zodiacName(int lunarYear);

void formatGanzhiYear(int lunarYear, char *out, size_t size);
void formatLunarDate(const LunarDate &date, char *out, size_t size);
void formatSolarTermInfo(const struct tm &now, char *out, size_t size);

// 公历日期相距 1970-01-01 的天数
int64_t daysFromCivil(int year, int month, int day);
